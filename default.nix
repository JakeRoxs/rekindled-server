# native deps
{
    lib
    , cmake
    , pkg-config
    , removeReferencesTo
    , writeShellApplication
    , jq
    , xz
    , runCommand
}:

# build deps
{
    stdenv
    , libuuid
    , sqlite
    , openssl
}:

with builtins;
with lib;

let
    # cpp-httplib: pre-fetch the source tree so the Nix build stays hermetic and
    # CMake's FetchContent consumes it via REKINDLED_HTTPLIB_SOURCE_DIR.
    httplibArchive = builtins.fetchurl {
      url = "https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.54.1.tar.gz";
      sha256 = "omipkmjocqrygdletm4o2auotw4gga5jphgl7w6rysyvot2cfx5q";
    };

    httplibSrc = runCommand "cpp-httplib-v0.54.1" {
      nativeBuildInputs = [ xz ];
    } ''
      tar -xzf ${httplibArchive}
      # The archive extracts to a single top-level directory; expose it as the
      # source tree consumed by FetchContent.
      mv cpp-httplib-0.54.1 $out
    '';

    opensslArchive = builtins.fetchurl {
      url = "https://github.com/kzalewski/openssl-1.1.1/archive/refs/tags/1.1.1ze.tar.gz";
      sha256 = "yzjxxnrt444sr3vagewltyi55e3y6kstz5zepciuqqeskcameuaq";
    };

    curlArchive = builtins.fetchurl {
      url = "https://curl.se/download/curl-8.1.0.tar.xz";
      sha256 = "npmavvhqogdqcwirefxoogc3sdjillcrmkxndppncrhz7ezdfi6a";
    };

    pkg = stdenv.mkDerivation rec {
        name = "rekindled-server";

        src = with fileset; toSource {
            root = ./.;
            fileset = unions [ ./CMakeLists.txt ./Source ./Tools/Build ];
        };

        nativeBuildInputs = [ cmake pkg-config removeReferencesTo xz ];
        buildInputs = [ libuuid sqlite openssl ];

        enableParallelBuilding = true;

        installPhase = ''
            cmake --install . --config Release --prefix "$out" --component Runtime
            '';

        # google's generated protobuf headers puts absolute file path garbage into the binaries
        # which will break reproducible builds.
        fixupPhase = ''
            find "$out" -type f -exec remove-references-to -t "${src}" '{}' +
            '';

        cmakeFlags = [
            "-DBUILD_TESTING=OFF"
            "-DCMAKE_BUILD_TYPE=Release"
            # Fix third party builds
            "-DCMAKE_C_STANDARD=99"
            "-DCMAKE_C_FLAGS=-Wno-implicit-function-declaration"

            # Prefer using FetchContent to download third-party sources at configure time
            # (falls back to system libraries if available).
            # When using Nix, supply the pre-fetched archive paths so CMake doesn't need network access.
            "-DREKINDLED_HTTPLIB_SOURCE_DIR=${httplibSrc}"
            "-DREKINDLED_OPENSSL_ARCHIVE=file://${opensslArchive}"
            "-DREKINDLED_CURL_ARCHIVE=file://${curlArchive}"
        ];

        # Can't pass multiple flags through cmakeFlags *sigh*
        # TODO: Nixpkgs now supports spaces in cmakeFlags when __structuredAttrs = true, so multiple flags can be passed.
        # https://github.com/NixOS/nixpkgs/issues/114044
        # Though these really should be fixed in rekindled itself
        NIX_CFLAGS_COMPILE = [ "-Wno-format-security" "-Wno-non-pod-varargs" ];

        meta = {
            homepage = "https://github.com/jakeroxs/rekindled-server";
            license = licenses.mit;
            platforms = [ "x86_64-linux" ];
        };
    };
in writeShellApplication {
    runtimeInputs = [ jq ];
    name = "rekindled-server";
    text = ''
        tmp="$(mktemp -d)"
        trap 'rm -rf "$tmp"' EXIT
        cd "$tmp"

        export LD_LIBRARY_PATH="${pkg}/lib:''${LD_LIBRARY_PATH:-}"
        config="''${XDG_CONFIG_HOME:-$HOME/.config/rekindled-server}"
        mkdir -p "$config"

        if [[ -f "$config/default/config.json" ]]; then
            game_type="$(jq -r '.GameType' "$config/default/config.json")"
        fi

        case "''${game_type:-DarkSouls3}" in
            DarkSouls2)
                echo "GameType: DarkSouls2"
                echo 335300 > steam_appid.txt
                ;;
            DarkSouls3)
                echo "GameType: DarkSouls3"
                echo 374320 > steam_appid.txt
                ;;
        esac

        ln -sf "${pkg}/share/rekindled-server/WebUI" .
        ln -sf "${pkg}/bin/Server" .
        ln -sf "$config" Saved
        exec ./Server "$@"
        '';
}
