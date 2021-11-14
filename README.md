# extio_rtl_tcp
ExtIO plugin for Winrad/HDSDR connecting to librtlsdr's rtl_tcp

Compilation with Microsoft Visual Studio Community 2019 (Desktop, C++),
but requiring platform toolset: Visual Studio 2017 - Windows XP (v141_xp)

Clone with submodules to automatically retrieve CLsocket from https://github.com/procitec/clsocket

```
git clone --recursive https://github.com/hayguen/extio_rtl_tcp.git
```

Having missed the `--recursive` option, you can fetch clsocket later with:

```
git submodule update --init
```

Check for `rtl_tcp_extio.cfg` - after having started and close HDSDR once.
You might edit bands and band specific settings there with a text editor.
Don't forget to `enable` the band configs, then restart HDSDR.


The CLsocket files are directly referenced from the ExtIO Visual Studio project.

Precompiled DLL is available here: https://github.com/hayguen/extio_rtl_tcp/releases
