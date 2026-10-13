#include <shared/economy.hh>

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
        for (const auto& other : kItems)
            if (&item != &other)
                require(item.action != other.action, "Duplicate purchase action");
    }
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
    sent.hasDefuseKit = true;
    sent.notice = "Bought MP5";
    sent.primary = "mp5";
    sent.secondary = "usp";
    sent.grenades = 2;
    sent.smokes = 1;
    Snapshot received;
    require(decode(encode(sent), received), "Economy update did not decode");
    require(received.money == sent.money && received.weapon == sent.weapon &&
                    received.hasDefuseKit == sent.hasDefuseKit &&
                    received.primary == sent.primary && received.secondary == sent.secondary &&
                    received.grenades == sent.grenades &&
                    received.smokes == sent.smokes &&
                    received.notice == sent.notice,
            "Economy update changed while decoding");
    sent.primary.clear();
    require(decode(encode(sent), received) && received.primary.empty() &&
                    received.secondary == "usp",
            "Empty primary slot did not decode");
    require(!decode(std::string(kMessagePrefix) + "-10 mp5 0 none", received),
            "Invalid balance was accepted");
    require(!decode(std::string(kMessagePrefix) + "800 usp 0 none - usp 3 0", received),
            "Excess grenades were accepted");
    require(!decode(std::string(kMessagePrefix) + "800 usp 0 none - usp 0 3", received),
            "Excess smoke grenades were accepted");

    GrenadeThrow thrown{42, 10.0f, 20.0f, 30.0f, 190.0f, 30.0f, true};
    GrenadeThrow shown;
    require(decodeGrenade(encodeGrenade(thrown), shown) && shown.owner == thrown.owner &&
                    shown.endX == thrown.endX && shown.endZ == thrown.endZ && shown.smoke,
            "Grenade throw event did not round trip");
}
