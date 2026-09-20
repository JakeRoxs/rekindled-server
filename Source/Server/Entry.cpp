/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Server/ServerManager.h"
#include "Config/BuildConfig.h"
#include "Shared/Core/Utils/Logging.h"
#include "Shared/Platform/Platform.h"

#include <filesystem>

int main(int argc, char* argv[]) {
// only do this on Windows, on Linux its very common to not have writable
// access to wherever the binary is installed
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  // Switch working directory to the same directory the
  // exe is inside of. Prevents wierdness when we start from visual studio etc.
  std::filesystem::path exe_directory = std::filesystem::path(argv[0]).parent_path();
  std::filesystem::current_path(exe_directory);
#endif

  Log(R"--(#     (           ) (       ) (     (       (      )--");
  Log(R"--(#     )\ )     ( /( )\ ) ( /( )\ )  )\ )    )\ )   )--");
  Log(R"--(#    (()/((    )\()|()/( )\()|()/( (()/( ( (()/(   )--");
  Log(R"--(#     /(_))\ |((_)\ /(_)|(_)\ /(_)) /(_)))\ /(_))  )--");
  Log(R"--(#    (_))((_)|_ ((_|_))  _((_|_))_ (_)) ((_|_))_   )--");
  Log(R"--(#    | _ \ __| |/ /|_ _|| \| ||   \| |  | __|   \  )--");
  Log(R"--(#    |   / _|  ' <  | | | .` || |) | |__| _|| |) | )--");
  Log(R"--(#    |(|_\___|(|\_\|___||_|\_(|___/|____|___|___/  )--");
  Log(R"--(#     )\ )    )\ )           )\ )                  )--");
  Log(R"--(#    (()/((  (()/((   (  (  (()/(                  )--");
  Log(R"--(#     /(_))\  /(_))\  )\ )\  /(_))                 )--");
  Log(R"--(#    (_))((_)(_))((_)((_|(_)(_))                   )--");
  Log(R"--(#    / __| __| _ \ \ / /| __| _ \                  )--");
  Log(R"--(#    \__ \ _||   /\ V / | _||   /                  )--");
  Log(R"--(#    |___/___|_|_\ \_/  |___|_|_\                  )--");
  Log(R"--(#                                                     )--");
  Log("");
  Log("https://github.com/jakeroxs/rekindled-server");
  Log("");

  if (!PlatformInit()) {
    Error("Failed to initialize platform specific functionality.");
    return 1;
  }

  ServerManager ServerManagerInstance;
  if (!ServerManagerInstance.Init()) {
    Error("Server failed to initialize.");
    return 1;
  }
  ServerManagerInstance.RunUntilQuit();
  if (!ServerManagerInstance.Term()) {
    Error("Server failed to terminate.");
    return 1;
  }

  if (!PlatformTerm()) {
    Error("Failed to tidy up platform specific functionality.");
    return 1;
  }

  return 0;
}
