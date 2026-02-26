use usbd_serial::embedded_io;

use crate::bsp::hal::{
    fugit,
    gpio::{Pin, PinId, PullDown, PullNone},
    pio::{
        self, InstallError, InstalledProgram, PIOBuilder, PIOExt, ShiftDirection, StateMachine,
        StateMachineIndex, UninitStateMachine,
    },
};

/// Install the GB Link Port leader program in a PIO instance
pub fn install_link_port_leader_program<PIO: PIOExt>(
    pio: &mut pio::PIO<PIO>,
) -> Result<LinkPortLeaderProgram<PIO>, InstallError> {
    let link_leader_def = pio_proc::pio_file!("./pio/link_port.pio", select_program("link_leader"));
    let program = link_leader_def.program;
    pio.install(&program)
        .map(|program| LinkPortLeaderProgram { program })
}

/// Represents the Leader implementation of a GB Link Port interface using the
/// RP2040's PIO hardware
///
/// # Type Parameters
/// - `SINID`: The PinId for the SIN pin.
/// - `SCKID`: The PinId for the SCK pin.
/// - `SOUTID`: The PinId for the SOUT pin.
/// - `SM`: The state machine to use.
/// - `State`: The state of the Link Port interface, either `pio::Stopped` or `pio::Running`.
pub struct PioGbLinkPortLeader<
    SINID: PinId,
    SCKID: PinId,
    SOUTID: PinId,
    PIO: PIOExt,
    SM: StateMachineIndex,
    State,
> {
    tx: pio::Tx<(PIO, SM)>,
    rx: pio::Rx<(PIO, SM)>,
    sm: StateMachine<(PIO, SM), State>,
    // The following fields are use to restore the original state in `free()`
    _sin_pin: Pin<SINID, PIO::PinFunction, PullDown>,
    _sck_pin: Pin<SCKID, PIO::PinFunction, PullNone>,
    _sout_pin: Pin<SOUTID, PIO::PinFunction, PullNone>,
}

/// Token of the already installed Link Port leader program. To be obtained with [`install_link_port_leader_program`].
pub struct LinkPortLeaderProgram<PIO: PIOExt> {
    program: InstalledProgram<PIO>,
}

impl<SINID: PinId, SCKID: PinId, SOUTID: PinId, PIO: PIOExt, SM: StateMachineIndex>
    PioGbLinkPortLeader<SINID, SCKID, SOUTID, PIO, SM, pio::Stopped>
{
    /// Create a new [`PioGbLinkPortLeader`] instance.
    /// Requires the [`LinkPortLeaderProgram`] to be already installed (see [`install_link_port_leader_program`]).
    ///
    /// # Arguments
    /// - `sin_pin`: The SIN pin configured with `FunctionPioX` and `PullDown`. Use [`pin.gpioX.reconfigure()`](https://docs.rs/rp2040-hal/latest/rp2040_hal/gpio/struct.Pin.html#method.reconfigure).
    /// - `sck_pin`: The SCK pin configured with `FunctionPioX` and `PullNone`. Use [`pin.gpioX.reconfigure()`](https://docs.rs/rp2040-hal/latest/rp2040_hal/gpio/struct.Pin.html#method.reconfigure).
    /// - `sout_pin`: The SOUT pin configured with `FunctionPioX` and `PullNone`. Use [`pin.gpioX.reconfigure()`](https://docs.rs/rp2040-hal/latest/rp2040_hal/gpio/struct.Pin.html#method.reconfigure).
    /// - `sm`: A PIO state machine instance.
    /// - `leader_program`: The installed Link Port leader program.
    /// - `sck_frequency`: Desired SCK frequency.
    /// - `system_freq`: System frequency.
    pub fn new(
        sin_pin: Pin<SINID, PIO::PinFunction, PullDown>,
        sck_pin: Pin<SCKID, PIO::PinFunction, PullNone>,
        sout_pin: Pin<SOUTID, PIO::PinFunction, PullNone>,
        sin_sm: UninitStateMachine<(PIO, SM)>,
        leader_program: &mut LinkPortLeaderProgram<PIO>,
        sck_freq: fugit::HertzU32,
        system_freq: fugit::HertzU32,
    ) -> Self {
        let div = system_freq / sck_freq;
        let sin_id = sin_pin.id().num;
        let sck_id = sck_pin.id().num;
        let sout_id = sout_pin.id().num;

        let (sm, rx, tx) =
            Self::build_pio(leader_program, sin_id, sck_id, sout_id, sin_sm, div as u16);

        Self {
            tx,
            sm,
            _sin_pin: sin_pin,
            _sck_pin: sck_pin,
            _sout_pin: sout_pin,
            rx,
        }
    }

    #[allow(clippy::type_complexity)]
    fn build_pio(
        token: &mut LinkPortLeaderProgram<PIO>,
        sin_id: u8,
        sck_id: u8,
        sout_id: u8,
        sm: UninitStateMachine<(PIO, SM)>,
        div: u16,
    ) -> (
        StateMachine<(PIO, SM), pio::Stopped>,
        pio::Rx<(PIO, SM)>,
        pio::Tx<(PIO, SM)>,
    ) {
        // SAFETY: Program can not be uninstalled, because it can not be accessed
        let program = unsafe { token.program.share() };
        let builder = PIOBuilder::from_installed_program(program);
        let (mut sm, rx, tx) = builder
            .set_pins(sck_id, 1)
            .in_pin_base(sin_id)
            .in_shift_direction(ShiftDirection::Left)
            .autopush(false)
            .push_threshold(8)
            .out_pins(sout_id, 1)
            .out_shift_direction(ShiftDirection::Left)
            .pull_threshold(8)
            .autopull(false)
            .buffers(pio::Buffers::RxTx)
            .clock_divisor_fixed_point(div, 0)
            .build(sm);
        sm.set_pindirs([
            (sin_id, pio::PinDir::Input),
            (sck_id, pio::PinDir::Output),
            (sout_id, pio::PinDir::Output),
        ]);
        (sm, rx, tx)
    }

    /// Enables the Link Port, transitioning it to the `Running` state.
    ///
    /// # Returns
    /// An instance of `PioGbLinkPortLeader` in the `Running` state.
    #[inline]
    pub fn enable(self) -> PioGbLinkPortLeader<SINID, SCKID, SOUTID, PIO, SM, pio::Running> {
        PioGbLinkPortLeader {
            sm: self.sm.start(),
            tx: self.tx,
            _sin_pin: self._sin_pin,
            _sck_pin: self._sck_pin,
            _sout_pin: self._sout_pin,
            rx: self.rx,
        }
    }
    /// Frees the underlying resources, returning the SM instance and link port pins.
    ///
    /// # Returns
    /// A tuple containing the used SM and the SIN, SCK, and SOUT pins.
    #[allow(clippy::type_complexity)]
    #[allow(unused)]
    pub fn free(
        self,
    ) -> (
        UninitStateMachine<(PIO, SM)>,
        Pin<SINID, PIO::PinFunction, PullDown>,
        Pin<SCKID, PIO::PinFunction, PullNone>,
        Pin<SOUTID, PIO::PinFunction, PullNone>,
    ) {
        let (tx_sm, _) = self.sm.uninit(self.rx, self.tx);
        (tx_sm, self._sin_pin, self._sck_pin, self._sout_pin)
    }
}

impl<SINID: PinId, SCKID: PinId, SOUTID: PinId, PIO: PIOExt, SM: StateMachineIndex>
    PioGbLinkPortLeader<SINID, SCKID, SOUTID, PIO, SM, pio::Running>
{
    /// Reads raw data into a buffer.
    ///
    /// # Arguments
    /// - `buf`: A mutable slice of u8 to store the read data.
    ///
    /// # Returns
    /// `Ok(usize)`: Number of bytes read.
    /// `Err(())`: If an error occurs.
    pub fn read_raw(&mut self, mut buf: &mut [u8]) -> Result<usize, ()> {
        let buf_len = buf.len();
        while let Some(b) = self.rx.read() {
            buf[0] = b as u8;
            buf = &mut buf[1..];
            if buf.is_empty() {
                break;
            }
        }
        Ok(buf_len - buf.len())
    }

    /// Writes raw data from a buffer.
    ///
    /// # Arguments
    /// - `buf`: A slice of u8 containing the data to write.
    ///
    /// # Returns
    /// `Ok(())`: On success.
    /// `Err(())`: If an error occurs.
    pub fn write_raw(&mut self, buf: &[u8]) -> Result<(), ()> {
        for b in buf {
            while self.tx.is_full() {
                core::hint::spin_loop()
            }
            if !self.tx.write(*b as u32) {
                return Err(());
            }
        }
        Ok(())
    }
    /// Flushes the Link Port transmit buffer.
    fn flush(&mut self) {
        while !self.tx.is_empty() {
            core::hint::spin_loop()
        }
    }
    /// Stops the Link Port, transitioning it back to the `Stopped` state.
    ///
    /// # Returns
    /// An instance of `PioGbLinkPortLeader` in the `Stopped` state.
    #[inline]
    #[allow(unused)]
    pub fn stop(self) -> PioGbLinkPortLeader<SINID, SCKID, SOUTID, PIO, SM, pio::Stopped> {
        PioGbLinkPortLeader {
            sm: self.sm.stop(),
            tx: self.tx,
            _sin_pin: self._sin_pin,
            _sck_pin: self._sck_pin,
            _sout_pin: self._sout_pin,
            rx: self.rx,
        }
    }
}

/// Represents errors that can occur in the PIO GB Link Port.
#[derive(core::fmt::Debug)]
#[non_exhaustive]
pub enum PioSerialError {
    /// General IO error
    IO,
}

impl embedded_io::Error for PioSerialError {
    fn kind(&self) -> embedded_io::ErrorKind {
        embedded_io::ErrorKind::Other
    }
}

impl<SINID: PinId, SCKID: PinId, SOUTID: PinId, PIO: PIOExt, SM: StateMachineIndex>
    embedded_io::ErrorType for PioGbLinkPortLeader<SINID, SCKID, SOUTID, PIO, SM, pio::Running>
{
    type Error = PioSerialError;
}

impl<SINID: PinId, SCKID: PinId, SOUTID: PinId, PIO: PIOExt, SM: StateMachineIndex>
    embedded_io::Read for PioGbLinkPortLeader<SINID, SCKID, SOUTID, PIO, SM, pio::Running>
{
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Self::Error> {
        self.read_raw(buf).map_err(|_| PioSerialError::IO)
    }
}

impl<SINID: PinId, SCKID: PinId, SOUTID: PinId, PIO: PIOExt, SM: StateMachineIndex>
    embedded_io::Write for PioGbLinkPortLeader<SINID, SCKID, SOUTID, PIO, SM, pio::Running>
{
    fn write(&mut self, buf: &[u8]) -> Result<usize, Self::Error> {
        self.write_raw(buf)
            .map(|_| buf.len())
            .map_err(|_| PioSerialError::IO)
    }
    fn flush(&mut self) -> Result<(), Self::Error> {
        self.flush();
        Ok(())
    }
}
