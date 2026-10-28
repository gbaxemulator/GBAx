GBAx
====

GBAx ist ein Emulator für Game Boy Advance-Spiele. Das Ziel von GBAx ist, schneller und genauer als viele existierende Game Boy Advance-Emulatoren zu sein. Außerdem verfügt GBAx über Funktionen, die anderen Emulatoren fehlen. Zusätzlich werden auch Game Boy- und Game Boy Color-Spiele unterstützt.

Aktuelle Neuigkeiten und Downloads findest Du auf [gbax-emulator.com](https://gbax-emulator.com).

[![Build-Status](https://travis-ci.org/gbaxemulator/gbax.svg?branch=master)](https://travis-ci.org/gbaxemulator/gbax)
[![Status der Übersetzungen](https://hosted.weblate.org/widgets/gbax/-/svg-badge.svg)](https://hosted.weblate.org/engage/gbax)

Features
--------

- Sehr genaue Unterstützung der Game Boy Advance-Hardware[<sup>[1]</sup>](#missing).
- Unterstützung der Game Boy-/Game Boy Color-Hardware.
- Schnelle Emulation. GBAx ist dafür bekannt, auch auf schwacher Hardware wie Netbooks mit voller Geschwindigkeit zu laufen.
- Qt- und SDL-Portierungen für eine vollwertige und eine "leichtgewichtige" Benutzeroberfläche.
- Lokale (gleicher Computer) Unterstützung für Link-Kabel.
- Erkennung des Speichertypes, einschließlich der Größe des Flash-Speichers[<sup>[2]</sup>](#flashdetect).
- Unterstützung für Spielmodule mit Bewegungssensoren und Rüttel-Effekten (nur verwendbar mit Spiele-Controllern).
- Unterstützung für Echtzeituhren, selbst ohne Konfiguration.
- Unterstützung für den Lichtsensor in Boktai-Spielen
- Unterstützung für Game Boy Printer und Game Boy Camera.
- Eingebaute BIOS-Implementierung mit der Möglichkeit, externe BIOS-Dateien zu laden.
- Turbo/Vorlauf-Unterstützung durch drücken der Tab-Taste.
- Rücklauf-Unterstützung durch drücken der Akzent-Taste.
- Frameskip von bis zu 10 Bildern.
- Unterstützung für Screenshots.
- Unterstützung für Cheat-Codes.
- 9 Speicherstände für Savestates/Spielzustände. Savestates können auch als Screenshots dargestellt werden.
- Video-, GIF-, WebP- und APNG-Aufzeichnung.
- e-Reader-Unterstützung.
- Frei wählbare Tastenbelegungen für Tastaturen und Controller.
- Unterstützung für ZIP- und 7z-Archive.
- Unterstützung für Patches im IPS-, UPS- und BPS-Format.
- Spiele-Debugging über ein Kommandozeilen-Interface und IDA Pro-kompatible GDB-Unterstützung.
- Einstellbare Rücklauf-Funktion.
- Unterstützung für das Laden und Exportieren von GameShark- und Action Replay-Abbildern.
- Verfügbare Cores für RetroArch/Libretro und OpenEmu.
- Übersetzungen für mehrere Sprachen über [Weblate](https://hosted.weblate.org/engage/gbax).
- Viele, viele kleinere Dinge.

### Game Boy-Mapper

Die folgenden Mapper werden vollständig unterstützt:

- MBC1
- MBC1M
- MBC2
- MBC3
- MBC3+RTC (MBC3+Echtzeituhr)
- MBC5
- MBC5+Rumble (MBC5+Rüttel-Modul)
- MBC7
- Wisdom Tree (nicht lizenziert)
- Pokémon Jade/Diamond (nicht lizenziert)
- BBD (nicht lizenziert, ählich MBC5)
- Hitek (nicht lizenziert, ähnlich MBC5)

Die folgenden Mapper werden teilweise unterstützt:

- MBC6 (fehlende Unterstützung für Schreibzugriffe auf den Flash-Speicher)
- MMM01
- Pocket Cam
- TAMA5 (fehlende RTC-Unterstützung)
- HuC-1 (fehlende Infrarot-Unterstützung)
- HuC-3 (fehlende RTC- und Infrarot-Unterstützung)

### Geplante Features

- Unterstützung für Link-Kabel-Multiplayer über ein Netzwerk.
- Unterstützung für Link-Kabel über Dolphin/JOY-Bus.
- MP2k-Audio-Abmischung für höhere Audio-Qualität als echte Hardware.
- Unterstützung für Tool-Assisted Speedruns.
- Lua-Unterstützung für Scripting.
- Eine umfangreiche Debugging-Suite.
- Unterstützung für Drahtlosadapter.

Unterstützte Plattformen
------------------------

- Windows 7 oder neuer
- OS X 10.9 (Mavericks)[<sup>[3]</sup>](#osxver) oder neuer
- Linux
- FreeBSD
- Nintendo 3DS
- Nintendo Switch
- Wii
- PlayStation Vita

Andere Unix-ähnliche Plattformen wie OpenBSD sind ebenfalls dafür bekannt, mit GBAx kompatibel zu sein. Sie sind jedoch nicht getestet und werden nicht voll unterstützt.

### Systemvoraussetzungen

Die Systemvoraussetzungen sind minimal. Jeder Computer, der mit Windows Vista oder neuer läuft, sollte in der Lage sein, die Emulation zu bewältigen. Unterstützung für OpenGL 1.1 oder neuer ist ebenfalls voraussgesetzt. OpenGL 3.2 oder neuer wird für Shader und erweiterte Funktionen benötigt.

Downloads
---------

Download-Links befinden sich in der [Downloads][downloads]-Sektion auf der offiziellen Website. Der Quellcode befindet sich auf [GitHub][source].

Steuerung
---------

Die Steuerung kann im Einstellungs-Menü konfiguriert werden. Viele Spiele-Controller werden automatisch erkannt und entsprechend belegt. Für Tastaturen wird standardmäßig folgende Belegung verwendet:

- **A**: X
- **B**: Z
- **L**: A
- **R**: S
- **Start**: Enter
- **Select**: Rücktaste

Kompilieren
-----------

Um GBAx kompilieren zu können, wird CMake 3.1 oder neuer benötigt. GCC und Clang sind beide dafür bekannt, GBAx kompilieren zu können. Visual Studio 2013 und älter funktionieren nicht. Unterstützung für Visual Studio 2015 und neuer wird bald hinzugefügt.

#### Kompilieren mit Docker

Der empfohlene Weg, um GBAx für die meisten Plattformen zu kompilieren, ist die Verwendung von Docker. Mehrere Docker-Images sind verfügbar, welche die benötigte Compiler-Umgebung und alle benötigten Abhängigkeiten beinhaltet, um GBAx für verschiedene Plattformen zu bauen.

Um ein Docker-Image zum Bau von GBAx zu verwenden, führe einfach folgenden Befehl in dem Verzeichnis aus, in welches Du den GBAx-Quellcode ausgecheckt hast:

	docker run --rm -t -v $PWD:/home/gbax/src gbax/windows:w32

Dieser Befehl erzeugt ein Verzeichnis `build-win32` mit den erzeugten Programmdateien. Ersetze `gbax/windows:32` durch ein Docker-Image für eine andere Plattform, wodurch dann das entsprechende Verzeichnis erzeugt wird. Die folgenden Docker-Images sind im Docker Hub verfügbar:

- gbax/3ds
- gbax/switch
- gbax/ubuntu:xenial
- gbax/ubuntu:bionic
- gbax/ubuntu:focal
- gbax/ubuntu:groovy
- gbax/vita
- gbax/wii
- gbax/windows:w32
- gbax/windows:w64

#### Unter *nix kompilieren

Verwende folgende Befehle, um GBAx mithilfe von CMake auf einem Unix-basierten System zu bauen:

	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
	make
	sudo make install

Damit wird GBAx gebaut und in `/usr/bin` und `/usr/lib` installiert. Installierte Abhängigkeiten werden automatisch erkannt. Features, die aufgrund fehlender Abhängigkeiten deaktiviert wurden, werden nach dem `cmake`-Kommando angezeigt.

Wenn Du macOS verwendest, sind die einzelnen Schritte etwas anders. Angenommen, dass Du den Homebrew-Paketmanager verwendest, werden folgende Schritte zum installieren der Abhängigkeiten und anschließenden bauen von GBAx empfohlen:

	brew install cmake ffmpeg libzip qt5 sdl2 libedit pkg-config
	mkdir build
	cd build
	cmake -DCMAKE_PREFIX_PATH=`brew --prefix qt5` ..
	make

Bitte beachte, dass Du unter macOS nicht `make install` verwenden solltest, da dies nicht korrekt funktionieren wird.

#### Für Entwickler: Kompilieren unter Windows

##### MSYS2

Um GBAx auf Windows zu kompilieren, wird MSYS2 empfohlen. Befolge die Installationsschritte auf der [MSYS2-Website](https://msys2.github.io). Stelle sicher, dass Du die 32-Bit-Version ("MSYS2 MinGW 32-bit") (oder die 64-Bit-Version "MSYS2 MinGW 64-bit", wenn Du GBAx für x86_64 kompilieren willst) verwendest und führe folgendes Kommando (einschließlich der Klammern) aus, um alle benötigten Abhängigkeiten zu installieren. Bitte beachte, dass dafür über 1100MiB an Paketen heruntergeladen werden, was eine Weile dauern kann:

	pacman -Sy --needed base-devel git ${MINGW_PACKAGE_PREFIX}-{cmake,ffmpeg,gcc,gdb,libelf,libepoxy,libzip,pkgconf,qt5,SDL2,ntldd-git}

Lade den aktuellen GBAx-Quellcode mithilfe des folgenden Kommandos herunter:

	git clone https://github.com/gbaxemulator/gbax.git

Abschließend wird GBAx über folgende Kommandos kompiliert:

	mkdir -p gbax/build
	cd gbax/build
	cmake .. -G "MSYS Makefiles"
	make -j$(nproc --ignore=1)

Bitte beachte, dass GBAx für Windows aufgrund der Vielzahl an benötigten DLLs nicht für die weitere Verteilung geeignet ist, wenn es auf diese Weise gebaut wurde. Es ist jedoch perfekt für Entwickler geeignet. Soll GBAx dennoch weiter verteilt werden (beispielsweise zu Testzwecken auf Systemen, auf denen keine MSYS2-Umgebung installiert ist), kann mithilfe des Befehls `cpack -G ZIP` ein ZIP-Archiv mit allen benötigten DLLs erstellt werden.

##### Visual Studio

GBAx mit Visual Studio zu bauen erfordert ein ähnlich kompliziertes Setup. Zuerst musst Du [vcpkg](https://github.com/Microsoft/vcpkg) installieren. Nachdem vcpkg installiert ist, musst Du noch folgende zusätzlichen Pakete installieren:

	vcpkg install ffmpeg[vpx,x264] libepoxy libpng libzip sdl2 sqlite3

Bitte beachte, dass diese Installation keine hardwarebeschleunigtes Video-Encoding auf Nvidia-Hardware unterstützen wird. Wenn Du darauf Wert legst, musst Du zuerst CUDA installieren und anschließend den vorherigen Befehl um `ffmpeg[vpx,x264,nvcodec]` ergänzen.

Zusätzlich wirst Du auch Qt installieren müssen. Unglücklicherweise steht für Qt kein Offline-Installationsprogramm für die jeweils aktuelle Version bereit. Daher musst Du entweder auf eine [ältere Version](https://download.qt.io/archive/qt/5.12/5.12.9/qt-opensource-windows-x86-5.12.9.exe) zurückgreifen (hierfür benötigst Du ein ansonsten nutzloses Benutzerkonto, aber Du kannst das umgehen, indem Du temporär einen ungültigen Netzwerk-Proxy hinterlegst oder über andere Methoden deine Netzwerkverbindung deaktivierst). Alternativ kannst Du auch den Online-Installer nutzen (für den ohnehin ein Benutzeraccount erfortderlich ist) oder Qt selbst mithilfe von vcpkg bauen (was verhältnismäßig lange dauert). Keine dieser Optionen ist besonders elegant. Bitte achte bei der Verwendung eines Installers darauf, die passende MSVC-Version zu wählen. Der Offline-Installer unterstützt aktuell noch nicht MSVC 2019. Die Installation mit vcpkg dauert ein wenig länger, besonders, wenn Du einen Computer mit vier oder weniger CPU-Cores nutzt:

    vcpkg install qt5-base qt5-multimedia

Öffne anschließend Visual Studio, wähle "Clone Repository" und gib dort `https://github.com/gbaxemulator/gbax.git` ein. Wenn Visual Studio das Repository geklont hat, gehe zu "Datei > CMake" und öffne die Datei CMakeLists.txt im Stammverzeichnis des ausgecheckten Repos. Anschließend kann GBAx in Visual Studio entwickelt werden, ähnlich wie andere Visual Studio CMake-Projekte.

#### Kompilieren mithilfe einer Toolchain

Wenn Du devkitARM (für 3DS), devkitPPC (für Wii), devkitA64 (für Switch) oder vitasdk (für PS Vita) installiert hast, kannst Du die folgenden Befehle zum Kompilieren verwenden:

	mkdir build
	cd build
	cmake -DCMAKE_TOOLCHAIN_FILE=../src/platform/3ds/CMakeToolchain.txt ..
	make
	
Ersetze den Parameter `-DCMAKE_TOOLCHAIN_FILE` dabei folgendermaßen:

- 3DS: `../src/platform/3ds/CMakeToolchain.txt`
- Switch: `../src/platform/switch/CMakeToolchain.txt`
- Vita: `../src/platform/psp2/CMakeToolchain.vitasdk`
- Wii: `../src/platform/wii/CMakeToolchain.txt`

### Abhängigkeiten

GBAx hat keine "harten" Abhängigkeiten. Dennoch werden die folgenden optionalen Abhängigkeiten für einige Features benötigt. Diese Features werden automatisch deaktiviert, wenn die benötigten Abhängigkeiten nicht gefunden werden.

- Qt 5: Für die Benutzeroberfläche. Qt Multimedia oder SDL werden für Audio-Ausgabe benötigt.
- SDL: Für eine einfachere Benutzeroberfläche und Spiele-Controller-Unterstützung in der Qt-Oberfläche. SDL 2 ist empfohlen, SDL 1.2 wird jedoch auch unterstützt.
- zlib und libpng: Für die Unterstützung von Bildschirmfotos und Savestates-in-PNG-Unterstützung.
- libedit: Für die Unterstützung des Kommandozeilen-Debuggers.
- ffmpeg oder libav: Für Videoaufzeichnungen.
- libzip oder zlib: Um ROMs aus ZIP-Dateien zu laden.
- SQLite3: Für Spiele-Datenbanken.
- libelf: Für das Laden von ELF-Dateien.

SQLite3, libpng und zlib werden mit dem Emulator mitgeliefert, sodass sie nicht zuerst kompiliert werden müssen.

Fußnoten
--------

<a name="missing">[1]</a> Zurzeit fehlende Features sind

- OBJ-Fenster für die Modi 3, 4 und 5 ([Bug #5](https://gbax-emulator.com/b/5))

<a name="flashdetect">[2]</a> In manchen Fällen ist es nicht möglich, die Größe des Flash-Speichers automatisch zu ermitteln. Diese kann dann zur Laufzeit konfiguriert werden, es wird jedoch empfohlen, den Fehler zu melden.

<a name="osxver">[3]</a> 10.9 wird nur für die Qt-Portierung benötigt. Es ist wahrscheinlich möglich, die Qt-Portierung unter macOS 10.7 und älter zu bauen und zu nutzen, aber das wird nicht offiziell unterstützt. Die SDL-Portierung ist dafür bekannt, mit 10.7 und möglicherweise auf älteren Versionen zu funktionieren.

[downloads]: https://gbax-emulator.com/downloads.html
[source]: https://github.com/gbaxemulator/gbax/

Copyright
---------

Copyright für GBAx © 2013 – 2021 Jeffrey Pfau. GBAx wird unter der [Mozilla Public License version 2.0](https://www.mozilla.org/MPL/2.0/) veröffentlicht. Eine Kopie der Lizenz ist in der mitgelieferten Datei LICENSE verfügbar.

GBAx beinhaltet die folgenden Bibliotheken von Drittanbietern:

- [inih](https://github.com/benhoyt/inih), Copyright © 2009 - 2020 Ben Hoyt, verwendet unter einer BSD 3-clause-Lizenz.
- [LZMA SDK](http://www.7-zip.org/sdk.html), Public Domain.
- [MurmurHash3](https://github.com/aappleby/smhasher), Implementierung von Austin Appleby, Public Domain.
- [getopt fot MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/), Public Domain.
- [SQLite3](https://www.sqlite.org), Public Domain.

Wenn Du ein Spiele-Publisher bist und GBAx für kommerzielle Verwendung lizenzieren möchtest, schreibe bitte eine e-Mail an [licensing@gbax-emulator.com](mailto:licensing@gbax-emulator.com) für weitere Informationen.
