#pragma once

// Shared engine startup/loop. Frontends supply UTF-8 arguments and select mode.
int RunCortexCommand(int argc, char** argv);
