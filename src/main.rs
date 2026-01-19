//! Runs a Serial TTY to GB link translation
//!
//! This allows a GB to act as an ANSI terminal (assuming software support on
//! the GB side)
#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use bsp::entry;
use rp2040_panic_usb_boot as _;

use heapless::spsc::{Producer, Queue};
use usb_device::{
    bus::UsbBusAllocator,
    device::{StringDescriptors, UsbDevice, UsbDeviceBuilder, UsbVidPid},
};
use usbd_serial::{embedded_io::Write, SerialPort};

#[cfg(feature = "rp-pico")]
pub use rp_pico as bsp;

#[cfg(feature = "waveshare-rp2040-zero")]
pub use waveshare_rp2040_zero as bsp;

use bsp::hal::{
    clocks::{init_clocks_and_plls, Clock},
    fugit,
    gpio::{FunctionPio0, Pin},
    pac::{self, interrupt, Interrupt},
    pio,
    sio::Sio,
    usb::UsbBus,
    watchdog::Watchdog,
};

mod link_port;

/// The USB Device Driver (shared with the interrupt).
static mut USB_DEVICE: Option<UsbDevice<bsp::hal::usb::UsbBus>> = None;

/// The USB Bus Driver (shared with the interrupt).
static mut USB_BUS: Option<UsbBusAllocator<bsp::hal::usb::UsbBus>> = None;

/// The USB Serial Device Driver (shared with the interrupt).
static mut USB_SERIAL: Option<SerialPort<bsp::hal::usb::UsbBus>> = None;

/// Link port TX queue
static mut LINK_PORT_BUFFER_WRITER: Option<Producer<'_, u8>> = None;

#[entry]
fn main() -> ! {
    let mut pac = pac::Peripherals::take().unwrap();
    let mut watchdog = Watchdog::new(pac.WATCHDOG);
    let sio = Sio::new(pac.SIO);

    let clocks = init_clocks_and_plls(
        bsp::XOSC_CRYSTAL_FREQ,
        pac.XOSC,
        pac.CLOCKS,
        pac.PLL_SYS,
        pac.PLL_USB,
        &mut pac.RESETS,
        &mut watchdog,
    )
    .ok()
    .unwrap();

    let pins = bsp::Pins::new(
        pac.IO_BANK0,
        pac.PADS_BANK0,
        sio.gpio_bank0,
        &mut pac.RESETS,
    );

    let gp0: Pin<_, FunctionPio0, _> = pins.gp0.into_function();
    let gp2: Pin<_, FunctionPio0, _> = pins.gp2.into_function();
    let gp3: Pin<_, FunctionPio0, _> = pins.gp3.into_function();

    let (mut pio0, _sm0, sm1, _sm2, _sm3) = pio::PIOExt::split(pac.PIO0, &mut pac.RESETS);
    let mut leader_program = link_port::install_link_port_leader_program(&mut pio0)
        .ok()
        .unwrap();

    let port = link_port::PioGbLinkPortLeader::new(
        gp0.reconfigure(),
        gp2.reconfigure(),
        gp3.reconfigure(),
        sm1,
        &mut leader_program,
        fugit::HertzU32::kHz(500).convert(),
        clocks.system_clock.freq(),
    );

    let usb_bus = UsbBusAllocator::new(UsbBus::new(
        pac.USBCTRL_REGS,
        pac.USBCTRL_DPRAM,
        clocks.usb_clock,
        true,
        &mut pac.RESETS,
    ));
    unsafe {
        // SAFETY: This is safe as interrupts haven't been started yet
        USB_BUS = Some(usb_bus);
    }

    // Grab a reference to the USB Bus allocator. We are promising to the
    // compiler not to take mutable access to this global variable whilst this
    // reference exists!
    let bus_ref = unsafe { USB_BUS.as_ref().unwrap() };

    let serial = SerialPort::new(bus_ref);
    unsafe {
        USB_SERIAL = Some(serial);
    }

    let mut usb_dev = UsbDeviceBuilder::new(bus_ref, UsbVidPid(0x2E8A, 0x000A))
        .strings(&[StringDescriptors::default()
            .manufacturer("GBTTY")
            .product("Serial port that outputs to Game Boy link port")
            .serial_number("TEST")])
        .unwrap()
        .device_class(2) // from: https://www.usb.org/defined-class-codes
        .build();
    let _ = usb_dev.force_reset();
    unsafe {
        // SAFETY: This is safe as interrupts haven't been started yet
        USB_DEVICE = Some(usb_dev);
    }

    let mut link_port_queue_reader = {
        let (p, c) = {
            static mut LINK_PORT_BUFFER_QUEUE: Queue<u8, 4096> = Queue::new();
            // SAFETY: `LINK_PORT_BUFFER_QUEUE` is only accessible in this scope
            // and `main` is only called once.
            #[allow(static_mut_refs)]
            unsafe {
                LINK_PORT_BUFFER_QUEUE.split()
            }
        };

        unsafe {
            LINK_PORT_BUFFER_WRITER = Some(p);
        };

        c
    };

    // Enable the USB interrupt
    unsafe {
        pac::NVIC::unmask(Interrupt::USBCTRL_IRQ);
    };

    let mut active_link_port = port.enable();

    loop {
        match link_port_queue_reader.peek() {
            None => {
                let _ = active_link_port.flush();
                cortex_m::asm::wfi();
            }
            Some(ch) => {
                let buf: [u8; 1] = [*ch];
                match active_link_port.write(&buf[..1]) {
                    Ok(_) => {
                        let _ = link_port_queue_reader.dequeue();
                    }
                    Err(_err) => {
                        // TODO: Log something?
                    }
                }
            }
        }

        // TODO: Send ENQ/WRU?
        // TODO: Read 0x55/0x66 for flow control?
    }
}

/// This function is called whenever the USB Hardware generates an Interrupt
/// Request.
///
/// We do all our USB work under interrupt, so the main thread can continue on
/// knowing nothing about USB.
#[allow(non_snake_case)]
#[interrupt]
fn USBCTRL_IRQ() {
    // SAFETY: These are safe to access because they're only accessed here
    // during an interrupt and are always set before enabling this interrupt
    let usb_dev = unsafe { USB_DEVICE.as_mut().unwrap() };
    let serial = unsafe { USB_SERIAL.as_mut().unwrap() };
    let link_port_queue = unsafe { LINK_PORT_BUFFER_WRITER.as_mut().unwrap() };

    // Poll the USB driver for new serial data
    if usb_dev.poll(&mut [serial]) {
        let mut buf = [0u8; 512];
        match serial.read(&mut buf) {
            Err(_e) => {
                // Do nothing
            }
            Ok(0) => {
                // Do nothing
            }
            Ok(count) => {
                let mut wr_ptr = &buf[..count];
                while !wr_ptr.is_empty() {
                    match link_port_queue.enqueue(wr_ptr[0]) {
                        Err(_) => {
                            let _ =
                                serial.write(b"\nWARNING: Unable to write to link port queue\n");
                        }
                        Ok(_) => {
                            // Do nothing. It worked, yay!
                        }
                    }

                    // Send back to the host for debugging
                    match serial.write(&wr_ptr[0..1]) {
                        Ok(len) => wr_ptr = &wr_ptr[len..],
                        // On error, just drop unwritten data.
                        // One possible error is Err(WouldBlock), meaning the USB
                        // write buffer is full.
                        Err(_) => break,
                    };
                }
            }
        }
    }
}
