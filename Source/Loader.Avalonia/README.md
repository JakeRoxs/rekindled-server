# Rekindled Avalonia Loader

## License

Licensed under [GNU GPL version 3](LICENSE). Includes SoulsFormatsNEXT (GPLv3)
and MIT-licensed code; see [MIT-NOTICE.txt](MIT-NOTICE.txt).

## Build and install

Use .NET SDK 10. From a checkout of the repository root, initialize the
dependencies and publish for your platform:

```sh
git submodule update --init --recursive
dotnet publish Source/Loader.Avalonia/Loader.Avalonia.csproj -c Release -r win-x64 --self-contained true -o artifacts/loader-avalonia
```

For Linux x64, substitute `linux-x64` for `win-x64`. Copy the entire publish
directory to the installation directory and run `Loader.Avalonia.exe` on
Windows or `./Loader.Avalonia` on Linux.

Game launching also needs the Windows `Injector.dll` beside the loader;
Linux additionally needs the external `proton-injector` helper. See the
repository root `README.md`, under "How do I build it?", for native build
prerequisites, combined builds, and packaging instructions. Publishing the
managed application alone does not build those native components.

## Distributing releases

Include license notices and provide the corresponding source for the released
version, including required submodule contents and build/install materials,
in accordance with GPLv3 section 6.
