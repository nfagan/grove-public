# grove

![screenshot](/images/screenshot1.png)

This is the public repository for grove, a sandbox video game about sound, the environment, and interactive dynamic systems.

This document is intended as a resource for people looking to build and run the program from source, and / or for developers interested in modifying or extending the program. For a non-technical introduction to this work, see the accompanying website: [https://grove.ooo](https://grove.ooo).

A pre-built binary and asset bundle for Windows is available [here](https://github.com/nfagan/grove-public/tree/main/releases) to demo (under releases).

# build for macos

**grove has only been tested on Apple-silicon (e.g. M1) based Macs.** If you have an Intel-based Mac and would consider attempting the following steps, I would be grateful to hear your experience. I am reachable here (you can file an issue) or via email: fagan dot nicholas at gmail.

## prerequesites

* git and Apple's developer tools
    * Open `terminal.app`. Type `git --version` and press enter. This checks to see if git is installed. If it is not, you will be prompted to install Apple's developer tools.
* [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)
    * [Download and install](https://sdk.lunarg.com/sdk/download/1.3.236.0/mac/vulkansdk-macos-1.3.236.0.dmg). **Important note**: This link points to the specific SDK version (1.3.236.0) I have validated with my M1-based machine. Newer SDK versions might work, but have not been tested. In the installer, when given the option, select the system-global installation option.
* cmake
    * [Download and install](https://cmake.org/download/). If given the option to add cmake to the path, click OK to enable this.

## build

1. Open a new terminal window. The following commands can be copied and pasted into this window.
2. Clone (download) this repository using git:
```bash
cd ~/Downloads
git clone --recursive https://github.com/nfagan/grove-public
```
3. Run the following to build the program:
```bash
cd ~/Downloads/grove-public && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 ..
cmake --build . --target vk_app --config Release -- -j 8
```
4. Run the following to run the program:
```bash
cd ~/Downloads/grove-public
./build/src/vk-app/vk_app -nt 1 -rd ./assets
```

# build for windows

**grove has only been tested on PCs with discrete GPUs from NVIDIA and AMD**. If you have a machine with Intel integrated graphics and would consider attempting the following steps, I would be grateful to hear your experience. I am reachable here (you can file an issue) or via email: fagan dot nicholas at gmail.

## prerequesites

* Microsoft Visual Studio
    * [Download and install](https://visualstudio.microsoft.com/). The community edition is free.
* git
    * [Download and install](https://git-scm.com/download/win). Note that this is not just git, but a terminal emulator used to build and run the program.
* [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)
    * [Download and install](https://sdk.lunarg.com/sdk/download/1.3.243.0/windows/VulkanRT-1.3.243.0-Installer.exe). **Important note**: This link points to the specific SDK version (1.3.243.0) I have validated with my machine. Newer SDK versions might work, but have not been tested. In the installer, when given the option, select the system-global installation option.

## build

1. Search the start menu for `git bash` and open a new terminal window. Clone (download) this repository using git:
```bash
cd ~/Downloads
git clone --recursive https://github.com/nfagan/grove-public
```
2. Launch Visual Studio and select "open a local folder". Navigate to your Downloads folder, and select the grove-public folder within.
3. In the top menu bar, click the little dropdown arrow next to "Select startup item," and select vk_app.exe. Then, to the left of this, click the button labeled x64-Debug and change it to x64-Release.
4. Press ctrl + b on the keyboard to build the program (or, in the top-most menu bar, click Build -> Build vk_app.exe).
5. In the terminal, run the following:
```bash
cd ~/Downloads/grove-public/ && ./install.sh
```
6. In Visual Studio, click the play button to run the program.
