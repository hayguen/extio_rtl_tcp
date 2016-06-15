
ExtIO Plugin for Winrad/HDSDR

An ExtIO is a DLL-plugin for an SDR application  and  is used for interfacing to receiver(s).
This ExtIO_RTL_TCP.dll connects via TCP (over network) to the program 'rtl_tcp'.
Rtl_tcp provides a network interface for an RTL-Dongle.

This DLL runs fine on Windows (x86/x64) with HDSDR; tested with Windows XP up to Windows 10.
This DLL runs fine on Linux (x86), running HDSDR with 'wine'.
This DLL should also run with other SDR applications supporting the ExtIO interface, but i have not tested.

You might install/run librtlsdr with rtl_tcp on a different PC or Raspberry Pi or similar.
This ExtIO does also control the tuner bandwidth. This requires a version of librtlsdr/rtl_tcp supporting this!,
e.g.  https://github.com/librtlsdr/librtlsdr  in the 'development' branch,
Windows binaries are available at
  https://github.com/librtlsdr/librtlsdr/releases/tag/win32_dev_2016-06-14
  https://github.com/librtlsdr/librtlsdr/releases/download/win32_dev_2016-06-14/librtlsdr-win32-2016-06-14.zip

For running rtl_tcp on Windows, you also need the Zadig driver. Have a look at
http://hdsdr.de/RTLSDR_with_HDSDR.pdf

Tuner bandwidth is supported with E4000 and R820T tuners.
Changing bandwidth with R820T looks good and shows immediate differences in spectrum.
E4000 is not so nice, i can't see any difference, for the bandwidths below 3400 kHz.
I also put in the wider bandwidths, even wider than the samplerate, which will produce aliasing!
These might be interesting for wide amplitude modulated signals, e.g. ADS-B.

Decimation of Samplerate is an option for very old and slow computers:
Before decimation a very simple and fast low-quality filter (sum) is applied:
  y(k) = sum over x( k * (1..decimation) )  ; x() = input samples from rtl_tcp
                                            ; y() = output samples to SDR app (HDSDR)
The sum produces values requiring more than 8 bit.
In addition to this soft-filter, minimize the tuner-bandwidth!
You will still receive aliases!!!, but a bit damped compared to directly sampling at the slower speed.
The reception's center frequency (LO) is the most alias-free region.


General information on RTL-SDR:
* http://www.rtl-sdr.com/
* http://sdr.osmocom.org/trac/wiki/rtl-sdr
* http://superkuh.com/rtlsdr.html


INSTALLATION:

Just copy the DLL MANUALLY into HDSDR's directory, usually C:\Program Files (x86)\HDSDR\.
Cause of privilege restrictions, you should first unpack somewhere else and copy there in a seperate step.
Do not unpack directly into HDSDR's directory!


SOURCE:

https://github.com/hayguen/extio_rtl_tcp

