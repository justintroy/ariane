# Third-Party Notices & Dependency Attributions

This document records the open-source software, fonts, and interface specifications used in the Ariane SA-MP editor fork.

---

## 1. librw

- **Project**: librw (RenderWare graphics and platform abstraction library)
- **Author**: aap and librw contributors
- **Website**: https://github.com/Southland-FR/librw
- **Pinned Commit**: `15ffa585216a9a7573ecc597b19ce2fde9b935f2` (branch: `ariane`)
- **License**: MIT License

```text
The MIT License (MIT)

Copyright (c) 2014 aap

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 2. JSON for Modern C++ (nlohmann/json)

- **Project**: JSON for Modern C++ (v3.11.3)
- **Author**: Niels Lohmann
- **Website**: https://github.com/nlohmann/json
- **Location**: `tools/euryopa/vendor/json.hpp`
- **License**: MIT License

```text
MIT License

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 3. MiniLZO

- **Project**: MiniLZO — lightweight subset of the LZO data compression library (v2.10)
- **Author**: Markus Franz Xaver Johannes Oberhumer
- **Website**: http://www.oberhumer.com/opensource/lzo/
- **Location**: `tools/euryopa/minilzo/`
- **License**: GNU General Public License v2.0 or later (GPL-2.0-or-later)
- **Copying & Notices**: See `tools/euryopa/minilzo/COPYING` and `tools/euryopa/minilzo/README.LZO`

Because this project links with MiniLZO, the distribution as a whole complies with the terms of the GNU General Public License.

---

## 4. Dear ImGui

- **Project**: Dear ImGui (bloat-free graphical user interface library for C++)
- **Author**: Omar Cornut and Dear ImGui contributors
- **Website**: https://github.com/ocornut/imgui
- **Location**: Bundled via `librw/skeleton/imgui`
- **License**: MIT License

```text
The MIT License (MIT)

Copyright (c) 2014-2024 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 5. ImGuizmo

- **Project**: ImGuizmo (Immediate mode 3D gizmo for scene editing)
- **Author**: Cedric Guillemet
- **Website**: https://github.com/CedricGuillemet/ImGuizmo
- **Location**: Bundled via `librw/skeleton/imgui`
- **License**: MIT License

```text
MIT License

Copyright (c) 2017 Cedric Guillemet

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 6. Inter Font

- **Project**: Inter font family
- **Designer**: Rasmus Andersson
- **Website**: https://rsms.me/inter/
- **Location**: `fonts/Inter-Regular.ttf`
- **License**: SIL Open Font License, Version 1.1 (OFL-1.1)

```text
Copyright (c) 2016-2024 The Inter Project Authors (https://github.com/rsms/inter)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is available with a FAQ at: https://openfontlicense.org
```

---

## 7. Font Awesome Free

- **Project**: Font Awesome Free (v6.x)
- **Author**: Fonticons, Inc.
- **Website**: https://fontawesome.com
- **Location**: `fonts/fa-solid-900.ttf`
- **License**: SIL Open Font License 1.1 (fonts), MIT License (code), CC BY 4.0 (icons)

```text
Font Awesome Free License

Font Awesome Free is free, open source, and GPL friendly. You can use it for
commercial projects, open source projects, or really almost whatever you want.

Font License: SIL OFL 1.1 (https://openfontlicense.org)
Code License: MIT License (https://opensource.org/licenses/MIT)
```

---

## 8. SA-MP & open.mp Scripting API Specifications

The static Pawn parser and code exporter implement syntactic and semantic compatibility with the scripting interfaces established by:

- **open.mp**: Community multiplayer platform for Grand Theft Auto: San Andreas (https://open.mp). Function signatures for `CreateObject`, `SetObjectMaterial`, `SetObjectMaterialText`, and `RemoveBuildingForPlayer` reference open.mp public scripting documentation (Mozilla Public License 2.0).
- **SA-MP Streamer Plugin**: Authored by Incognito (https://github.com/samp-incognito/samp-streamer-plugin). Function signatures and parameter ordering for `CreateDynamicObject`, `SetDynamicObjectMaterial`, and `SetDynamicObjectMaterialText` reference `streamer.inc`.

*Note: Ariane does not embed, distribute, or execute a SA-MP or open.mp server runtime.*

---

## 9. Python MCP SDK

- **Project**: Model Context Protocol Python SDK
- **Author**: Anthropic, PBC
- **Website**: https://github.com/modelcontextprotocol/python-sdk
- **License**: MIT License

---

## 10. Game Intellectual Property Disclaimer

- **Grand Theft Auto**, **GTA San Andreas**, **GTA Vice City**, and **GTA III** are registered trademarks and copyrights of **Rockstar Games, Inc.** and **Take-Two Interactive Software, Inc.**
- **Ariane** is an independent, community-developed map editor tool and is **NOT** affiliated with, endorsed by, or sponsored by Rockstar Games or Take-Two Interactive.
- **No Proprietary Game Assets Included**: This repository, build scripts, and distributed archives do **NOT** bundle, distribute, or download any copyrighted assets from Grand Theft Auto (including but not limited to 3D models `.dff`, texture archives `.txd`, collision archives `.col`, placement files `.ipl`, definition files `.ide`, or game image archives `.img`).
- Users must supply their own legally purchased copy of Grand Theft Auto to use the editor.
