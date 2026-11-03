# GLFW 3.5.1

창 · 입력 · GL 컨텍스트. `SimpleGame.vcxproj` 가 이 폴더만 본다 - 따로 설치할 것 없다.

- 출처: <https://github.com/glfw/glfw/releases/tag/3.5.1> 의 `glfw-3.5.1.bin.WIN64.zip` · `glfw-3.5.1.bin.WIN32.zip`
  - WIN64 sha256 `34c6766043e09ad9c6e88b5636a5f34d1f2857aae828768c5bf2016dc8821a93`
  - WIN32 sha256 `47ee7ce5307e11f8515a2bb2d6ace6d88ffd6dbcd614b87da21969261f3d0ffe`
- `include/GLFW/` 는 zip 의 `include/GLFW/` 그대로(두 zip 이 같다)
- `x64/` · `Win32/` 는 각 zip 의 `lib-vc2022/` 에서 `glfw3dll.lib` · `glfw3.dll` 만. 폴더 이름이 `$(Platform)` 과 같아야 한다

**DLL 로 쓰는 이유.** 정적 `glfw3.lib` 는 `/DEFAULTLIB:MSVCRT` 를 박고 있어서 Debug(`/MDd`)에 넣으면 런타임 둘이 섞인다(LNK4098).
DLL 은 자기 런타임을 따로 들고 있어 구성과 상관없다. `glfw3.dll` 은 빌드 때 `CopyFileToFolders` 가 `$(OutDir)` 로 복사한다.

**올릴 때.** 새 zip 에서 같은 파일만 갈아 끼우고 위 버전 · 해시를 고친다. 소스 쪽 정의는 `GLFW_DLL` 하나다.
