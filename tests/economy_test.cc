#include <shared/economy.hh>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{
void require(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
}

int main()
{
    using namespace nx3d::economy;
    require(kItems.size() == 11, "Buy catalog changed unexpectedly");
    require(defaultWeapon("Terrorist") == "glock" &&
                    defaultWeapon("Counter Terrorist") == "usp",
            "Starter pistols do not match teams");
    require(isSecondary("glock") && isSecondary("usp") && isSecondary("deagle") &&
                    !isSecondary("mp5") && !isSecondary("ak47"),
            "Weapon slot classification is wrong");
    require(itemForAction(19)->kind == ItemKind::grenade &&
                    itemForAction(20)->kind == ItemKind::grenade &&
                    itemForAction(kThrowGrenadeAction) == nullptr &&
                    itemForAction(kThrowSmokeAction) == nullptr,
            "Grenade purchase and throw actions overlap");
    for (const auto& item : kItems)
    {
        require(itemForAction(item.action) == &item && item.price > 0,
                "Catalog action or price is invalid");
        require(item.object == nx3d::findNXObjectForPurchase(item.action) &&
                    item.object->price == item.price,
                "Purchase does not reference its NXObject");
        for (const auto& other : kItems)
            if (&item != &other)
                require(item.action != other.action, "Duplicate purchase action");
    }
    for (const auto& object : nx3d::kNXObjects)
    {
        require(nx3d::findNXObject(object.id) == &object,
                "NXObject lookup failed");
        for (const auto path : {object.visual.worldModel, object.visual.worldMaterial,
                                object.visual.viewModel, object.visual.viewMaterial})
            require(!path.empty() &&
                        std::filesystem::exists(std::filesystem::path(PROJECT_ROOT) /
                                                "assets" / path),
                    "NXObject is missing a 3D model or material");
        for (const auto& other : nx3d::kNXObjects)
            if (&object != &other)
                require(object.id != other.id, "Duplicate NXObject ID");
    }
    require(nx3d::findNXObject("bomb")->slot == 6 &&
                nx3d::findNXObject("grenade")->slot == 4 &&
                nx3d::findNXObject("smoke")->slot == 5 &&
                nx3d::findNXObject("defuse_kit")->slot == 7,
            "Objective and throwables have incorrect NXObject slots");
    require(inBuyZone("Counter Terrorist", 355.0f, -48.0f, -2364.0f),
            "Counter Terrorist spawn missing from buy zone");
    require(!inBuyZone("Terrorist", 355.0f, -48.0f, -2364.0f),
            "Terrorist can buy at the opposing spawn");
    require(inBuyZone("Terrorist", -592.0f, 192.0f, 764.0f),
            "Terrorist spawn missing from buy zone");
    require(!inBuyZone("Counter Terrorist", -592.0f, 192.0f, 764.0f),
            "Counter Terrorist can buy at the opposing spawn");
    require(!inBuyZone("Terrorist", 0.0f, 0.0f, 0.0f),
            "Remote purchase was allowed");

    Snapshot sent;
    sent.money = 1200;
    sent.weapon = "mp5";
    require(sent.objects.add("defuse_kit"), "Could not add defuse kit NXObject");
    sent.notice = "Bought MP5";
    sent.primary = "mp5";
    sent.secondary = "usp";
    require(sent.objects.add("grenade") && sent.objects.add("grenade") &&
                    !sent.objects.add("grenade") && sent.objects.add("smoke") &&
                    sent.objects.add("bomb"),
            "NXObject stack limits or ownership failed");
    Snapshot received;
    require(decode(encode(sent), received), "Economy update did not decode");
    require(received.money == sent.money && received.weapon == sent.weapon &&
                    received.objects.count("defuse_kit") == 1 &&
                    received.primary == sent.primary && received.secondary == sent.secondary &&
                    received.objects.count("grenade") == 2 &&
                    received.objects.count("smoke") == 1 &&
                    received.objects.count("bomb") == 1 &&
                    received.notice == sent.notice,
            "Economy update changed while decoding");
    sent.primary.clear();
    require(decode(encode(sent), received) && received.primary.empty() &&
                    received.secondary == "usp",
            "Empty primary slot did not decode");
    require(received.objects.consume("smoke") &&
                    received.objects.count("smoke") == 0 &&
                    !received.objects.consume("smoke"),
            "Smoke NXObject did not consume exactly once");
    require(!decode(std::string(kMessagePrefix) + "-10 mp5 0 none", received),
            "Invalid balance was accepted");
    require(!decode(std::string(kMessagePrefix) + "800 usp 0 none - usp 3 0", received),
            "Excess grenades were accepted");
    require(!decode(std::string(kMessagePrefix) + "800 usp 0 none - usp 0 3", received),
            "Excess smoke grenades were accepted");

    GrenadeThrow thrown{42, 10.0f, 20.0f, 30.0f, 190.0f, 30.0f, "smoke"};
    GrenadeThrow shown;
    require(decodeGrenade(encodeGrenade(thrown), shown) && shown.owner == thrown.owner &&
                    shown.endX == thrown.endX && shown.endZ == thrown.endZ &&
                    shown.objectId == "smoke",
            "Grenade throw event did not round trip");
    thrown.objectId = "bomb";
    require(!decodeGrenade(encodeGrenade(thrown), shown),
            "Non-throwable NXObject was accepted as a grenade event");
}
