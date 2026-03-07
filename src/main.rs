//! Runs a Serial TTY to GB link translation
//!
//! This allows a GB to act as an ANSI terminal (assuming software support on
//! the GB side)
#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use bsp::entry;
use defmt::*;
use defmt_rtt as _;
use rp2040_panic_usb_boot as _;

use heapless::spsc::{Producer, Queue};
use usb_device::{
    bus::UsbBusAllocator,
    device::{StringDescriptors, UsbDevice, UsbDeviceBuilder, UsbVidPid},
};
use usbd_serial::{
    embedded_io::{Read, Write},
    SerialPort,
};

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

/// Link port TX queue writer
static mut LINK_PORT_BUFFER_WRITER: Option<Producer<'_, u8>> = None;

const LINK_PORT_CLOCK_HZ: u32 = 262144; // ~32KB/s on normal speed

const LINK_PORT_ACK: u8 = 0x6;

const LINK_PORT_SYN_IDLE: u8 = 0x16;

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

    let clock_cycles_per_ms = clocks.system_clock.freq().to_Hz() / 1000;

    let pins = bsp::Pins::new(
        pac.IO_BANK0,
        pac.PADS_BANK0,
        sio.gpio_bank0,
        &mut pac.RESETS,
    );

    #[cfg(feature = "rp-pico")]
    let (sin_pin, sck_pin, sout_pin) = {
        let gp0: Pin<_, FunctionPio0, _> = pins.gp0.into_function();
        let gp1: Pin<_, FunctionPio0, _> = pins.gp1.into_function();
        let gp2: Pin<_, FunctionPio0, _> = pins.gp2.into_function();
        (gp0, gp1, gp2)
    };

    #[cfg(feature = "waveshare-rp2040-zero")]
    let (sin_pin, sck_pin, sout_pin) = {
        let gp0: Pin<_, FunctionPio0, _> = pins.gp0.into_function();
        let gp2: Pin<_, FunctionPio0, _> = pins.gp2.into_function();
        let gp3: Pin<_, FunctionPio0, _> = pins.gp3.into_function();
        (gp0, gp2, gp3)
    };

    let (mut pio0, sm0, _sm1, _sm2, _sm3) = pio::PIOExt::split(pac.PIO0, &mut pac.RESETS);
    let mut leader_program = link_port::install_link_port_leader_program(&mut pio0)
        .ok()
        .unwrap();

    let port = link_port::PioGbLinkPortLeader::new(
        sin_pin.reconfigure(),
        sck_pin.reconfigure(),
        sout_pin.reconfigure(),
        sm0,
        &mut leader_program,
        fugit::HertzU32::Hz(LINK_PORT_CLOCK_HZ).convert(),
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
                let idle = [LINK_PORT_SYN_IDLE];
                match active_link_port.write(&idle) {
                    Ok(_) => {
                        let mut resp = [0u8];
                        match active_link_port.read(&mut resp) {
                            Ok(1) => match resp[0] {
                                LINK_PORT_SYN_IDLE | LINK_PORT_ACK | 0x00 | 0xFF => {
                                    // Let's both twiddle our thumbs in silence
                                }
                                _ => {
                                    // SAFETY: While not totally safe, we're only writing from
                                    // here and reading in the interrupt. R/W buffers are seperate.
                                    // USB Bus is mutexed. It's fine for now :shruggie:
                                    let serial = unsafe { USB_SERIAL.as_mut().unwrap() };
                                    if serial.write(&resp).is_err() {
                                        error!("unable to enqueue to serial port");
                                    }
                                }
                            },
                            Ok(_) => {}
                            Err(_) => error!("pio error reading from link port"),
                        }
                    }
                    Err(_err) => error!("unable to write SYN_IDLE to link port"),
                }
            }
            Some(ch) => {
                let buf = [*ch];
                match active_link_port.write(&buf) {
                    Ok(_) => {
                        let _ = link_port_queue_reader.dequeue();

                        let mut resp = [0u8];
                        match active_link_port.read(&mut resp) {
                            Ok(1) => {
                                match resp[0] {
                                    LINK_PORT_SYN_IDLE | LINK_PORT_ACK | 0x00 | 0xFF => {
                                        // Let's both twiddle our thumbs in silence
                                    }
                                    _ => {
                                        // SAFETY: While not totally safe, we're only writing from
                                        // here and reading in the interrupt. R/W buffers are seperate.
                                        // USB Bus is mutexed. It's fine for now :shruggie:
                                        let serial = unsafe { USB_SERIAL.as_mut().unwrap() };
                                        if serial.write(&resp).is_err() {
                                            error!("unable to enqueue to serial port");
                                        }
                                    }
                                }
                            }
                            Ok(_) => {}
                            Err(_) => error!("pio error reading from link port"),
                        }
                    }
                    Err(_err) => error!("unable to write byte from queue to link port"),
                }
            }
        }

        cortex_m::asm::delay(clock_cycles_per_ms);
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

    if usb_dev.poll(&mut [serial]) {
        let mut buf = [0u8; 128];
        match serial.read(&mut buf) {
            Err(_e) => error!("usb error reading from serial port"),
            Ok(0) => {}
            Ok(count) => {
                for byte in &buf[..count] {
                    if link_port_queue.enqueue(*byte).is_err() {
                        error!("unable to write byte to link port queue");
                    }
                }
            }
        }
    }
}
