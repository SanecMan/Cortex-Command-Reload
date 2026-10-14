#include "../Source/System/ShotFlashPresentation.h"
#include <iostream>
int main() {
    RTE::ShotFlashPresentation flash;
    if (flash.Visible(0)) return 1;
    flash.Observe(0, 65535, 1000);
    if (flash.Visible(1000)) return 2;
    flash.Observe(1, 33, 1000);
    if (!flash.Visible(1000) || !flash.Visible(1066) || flash.Visible(1067)) return 3;
    flash.Observe(1, 0, 1060); // Interpolation and duplicate packets cannot extend it.
    if (flash.Visible(1067)) return 4;
    flash.Observe(2, 150, 2000);
    if (flash.Visible(2000)) return 5;
    flash.Observe(4, 20, 3000); // One or more dropped snapshots.
    if (!flash.Visible(3079) || flash.Visible(3080)) return 6;
    flash.Observe(0xFFFFFFFFU, 0, 4000);
    flash.Observe(0, 0, 4100); // Counter wrap is a new event too.
    if (!flash.Visible(4199) || flash.Visible(4200)) return 7;
    RTE::ShotFlashPresentation lateJoin;
    lateJoin.Observe(100, 65535, 5000);
    if (lateJoin.Visible(5000)) return 8;
    std::cout << "shot_flash_presentation=passed\n";
}
