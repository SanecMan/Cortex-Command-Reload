#pragma once

#include "GameActivity.h"
#include "BuyMenuGUI.h"
#include "ACDropShip.h"
#include "PresetMan.h"
#include "ThreadMan.h"
#include <cmath>
#include <limits>
#include <ostream>

namespace RTE {
	// Only called by the explicit diagnostic fixture. It mutates this disposable activity's economy.
	inline bool RunDeliveryValidationTest(GameActivity& activity, std::ostream& log) {
		constexpr int player = Players::PlayerOne;
		const int team = activity.GetTeamOfPlayer(player);
		BuyMenuGUI* menu = activity.GetBuyGUI(player);
		const auto* craft = dynamic_cast<const ACDropShip*>(g_PresetMan.GetEntityPreset("ACDropShip", "Dropship MK1", "Base.rte"));
		const auto* item = dynamic_cast<const SceneObject*>(g_PresetMan.GetEntityPreset("AHuman", "Fat Culled Clone", "Base.rte"));
		if (!menu || !craft || !item || team < 0 || team >= Activity::Teams::MaxTeamCount) return false;
		const std::string craftName = craft->GetModuleAndPresetName(), itemName = item->GetModuleAndPresetName();
		menu->ClearAllowedItems();
		menu->ClearProhibitedItems();
		menu->ClearAlwaysAllowedItems();
		menu->SetEnforceMaxMassConstraint(false);
		menu->SetEnforceMaxPassengersConstraint(false);
		menu->SetOnlyShowOwnedItems(false);
		menu->SetOwnedItemsAmount(craftName, 0);
		menu->SetOwnedItemsAmount(itemName, 1);
		activity.ClearOverridePurchase(player);
		Vector landingZone(400, 30);
		activity.SetLZArea(team, Scene::Area());
		const std::vector<const SceneObject*> cargo{item, item};
		float charged = 0;
		std::string error;
		const int initialDeliveries = activity.GetDeliveryCount(team);
		auto rejected = [&](const std::vector<const SceneObject*>& order, const Vector& lz, int mode, const char* expected) {
			const float funds = activity.GetTeamFunds(team);
			const int stock = menu->GetOwnedItemsAmount(itemName), craftStock = menu->GetOwnedItemsAmount(craftName);
			const Vector previousLZ = activity.GetLandingZone(player);
			charged = 123;
			const bool result = !activity.CreateValidatedDelivery(player, craft, order, lz, mode, charged, error) &&
			    error == expected && charged == 0 && funds == activity.GetTeamFunds(team) &&
			    stock == menu->GetOwnedItemsAmount(itemName) && craftStock == menu->GetOwnedItemsAmount(craftName) &&
			    initialDeliveries == activity.GetDeliveryCount(team) && previousLZ == activity.GetLandingZone(player);
			log << "delivery_rejection=" << expected << " result=" << (result ? "passed" : "failed") << " actual=" << error << '\n';
			return result;
		};
		bool passed = true;
		activity.SetTeamFunds(100000, team);
		passed = rejected(cargo, Vector(std::numeric_limits<float>::quiet_NaN(), 30), Actor::AIMODE_SENTRY, "invalid_landing_zone") && passed;
		passed = rejected(cargo, landingZone, Actor::AIMODE_COUNT, "invalid_ai_mode") && passed;
		passed = rejected({nullptr}, landingZone, Actor::AIMODE_SENTRY, "invalid_preset") && passed;
		passed = rejected(std::vector<const SceneObject*>(129, item), landingZone, Actor::AIMODE_SENTRY, "order_limit") && passed;
		menu->AddProhibitedItem(itemName);
		passed = rejected(cargo, landingZone, Actor::AIMODE_SENTRY, "prohibited_item") && passed;
		menu->RemoveProhibitedItem(itemName);
		menu->AddAllowedItem(craftName);
		passed = rejected(cargo, landingZone, Actor::AIMODE_SENTRY, "prohibited_item") && passed;
		menu->ClearAllowedItems();
		menu->SetOnlyShowOwnedItems(true);
		menu->SetOwnedItemsAmount(craftName, 1);
		passed = rejected(cargo, landingZone, Actor::AIMODE_SENTRY, "out_of_stock") && passed;
		menu->SetOnlyShowOwnedItems(false);
		menu->SetOwnedItemsAmount(craftName, 0);
		menu->SetEnforceMaxMassConstraint(true);
		passed = rejected(std::vector<const SceneObject*>(128, item), landingZone, Actor::AIMODE_SENTRY, "mass_limit") && passed;
		menu->SetEnforceMaxMassConstraint(false);
		menu->SetEnforceMaxPassengersConstraint(true);
		passed = rejected(std::vector<const SceneObject*>(128, item), landingZone, Actor::AIMODE_SENTRY, "passenger_limit") && passed;
		menu->SetEnforceMaxPassengersConstraint(false);
		activity.SetTeamFunds(0, team);
		passed = rejected(cargo, landingZone, Actor::AIMODE_SENTRY, "insufficient_funds") && passed;

		activity.SetTeamFunds(100000, team);
		// Preserve a trusted Lua override while committing the validated order.
		activity.AddOverridePurchase(craft, player);
		g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
		const float expectedCost = craft->GetGoldValue(0, 1, 1) + item->GetGoldValue(0, 1, 1);
		const bool accepted = activity.CreateValidatedDelivery(player, craft, cargo, landingZone, Actor::AIMODE_SENTRY, charged, error);
		const bool economyPassed = accepted && error.empty() && std::abs(charged - expectedCost) < 0.01F &&
		    std::abs(activity.GetTeamFunds(team) - (100000 - expectedCost)) < 0.01F &&
		    menu->GetOwnedItemsAmount(itemName) == 0 && activity.GetDeliveryCount(team) == initialDeliveries + 1;
		const bool overridePassed = accepted && activity.CreateDelivery(player) && activity.GetDeliveryCount(team) == initialDeliveries + 2;
		log << "validated_delivery_economy=" << (economyPassed ? "passed" : "failed") << " charged=" << charged << " expected=" << expectedCost
		    << " error=" << error << " lua_override_preserved=" << overridePassed << '\n' << std::flush;
		return passed && economyPassed && overridePassed;
	}
}
