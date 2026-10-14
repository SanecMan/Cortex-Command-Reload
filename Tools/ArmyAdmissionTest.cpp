#include <lua.hpp>
#include <iostream>

static bool Run(lua_State* state, const char* code) {
	if (luaL_dostring(state, code) == 0) return true;
	std::cerr << lua_tostring(state, -1) << '\n';
	return false;
}

int main() {
	for (bool diggers : {false, true}) {
		lua_State* state = luaL_newstate();
		if (!state) return 1;
		luaL_openlibs(state);
		if (!Run(state, R"(
OneManArmy = {}
GameActivity = {CAKEDIFFICULTY=20, EASYDIFFICULTY=40, MEDIUMDIFFICULTY=60,
                HARDDIFFICULTY=80, NUTSDIFFICULTY=95, MAXDIFFICULTY=100}
Activity = {RUNNING=4, OVER=6, PLAYER_1=0, MAXPLAYERCOUNT=4}
)")) return 2;
		const char* path = diggers ? "Data/Base.rte/Activities/OneManArmyDiggers.lua" : "Data/Base.rte/Activities/OneManArmy.lua";
		if (luaL_dofile(state, path) != 0) {
			std::cerr << lua_tostring(state, -1) << '\n'; return 3;
		}
		lua_pushboolean(state, diggers); lua_setglobal(state, "diggers");
		if (!Run(state, R"(
for difficulty = 0, 100 do
  OneManArmy.Difficulty = difficulty
  local actor, primary, secondary = OneManArmy:GetHumanLoadoutGroups()
  local a, p, s
  if diggers then
    a,p,s = "Actors - Light", "Weapons - Primary", "Weapons - Secondary"
    if difficulty <= 20 then a,p,s = "Actors - Heavy", "Weapons - Heavy", "Weapons - Explosive"
    elseif difficulty <= 40 then a,p,s = "Actors - Heavy", "Weapons - Primary", "Weapons - Light"
    elseif difficulty <= 60 then
    elseif difficulty <= 80 then p = "Weapons - Light"
    elseif difficulty <= 95 then p = "Weapons - Secondary"
    else p,s = "Weapons - Secondary", "Tools" end
  else
    a,p,s = "Actors - Heavy", "Weapons - Heavy", "Weapons - Light"
    if difficulty <= 20 then s = "Weapons - Explosive"
    elseif difficulty <= 80 then
    elseif difficulty <= 95 then a,p,s = "Actors - Light", "Weapons - Primary", "Weapons - Secondary"
    else a,p,s = "Actors - Light", "Weapons - Secondary", "Weapons - Secondary" end
  end
  assert(actor == a and primary == p and secondary == s, "loadout changed at " .. difficulty)
end
-- The real setup loop must inspect only the requested player.
local queried = {}
function OneManArmy:PlayerActive(player) return true end
function OneManArmy:PlayerHuman(player) return true end
function OneManArmy:GetPlayerBrain(player) queried[#queried+1] = player; return {} end
OneManArmy:SetupHumanPlayerBrains("a", "p", "s", 2)
assert(#queried == 1 and queried[1] == 2)
queried = {}
OneManArmy:SetupHumanPlayerBrains("a", "p", "s")
assert(#queried == 4, "initial startup no longer initializes all players")
-- Joining must preserve difficulty groups and pass the individual slot.
local calls = 0
function OneManArmy:SetupHumanPlayerBrains(a, p, s, player)
  local ea,ep,es = self:GetHumanLoadoutGroups()
  assert(a == ea and p == ep and s == es and player == 2)
  calls = calls + 1
end
OneManArmy.ActivityState = Activity.RUNNING
OneManArmy:OnPlayerJoined(2)
assert(calls == 1)
OneManArmy.ActivityState = Activity.OVER
OneManArmy:OnPlayerJoined(2)
OneManArmy.ActivityState = 2
OneManArmy:OnPlayerJoined(2)
assert(calls == 1, "joining resurrected a completed or editing match")
)")) return 4;
		lua_close(state);
	}
	std::cout << "Army loadouts and admission tests passed\n";
}
