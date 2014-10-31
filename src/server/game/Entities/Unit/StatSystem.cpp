/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Unit.h"
#include "Player.h"
#include "Pet.h"
#include "Creature.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "World.h"

inline bool _ModifyUInt32(bool apply, uint32& baseValue, int32& amount)
{
    // If amount is negative, change sign and value of apply.
    if (amount < 0)
    {
        apply = !apply;
        amount = -amount;
    }
    if (apply)
        baseValue += amount;
    else
    {
        // Make sure we do not get uint32 overflow.
        if (amount > int32(baseValue))
            amount = baseValue;
        baseValue -= amount;
    }
    return apply;
}

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########                         ########
#######################################*/

bool Player::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);

    SetStat(stat, int32(value));

    if (stat == STAT_STAMINA || stat == STAT_INTELLECT || stat == STAT_STRENGTH)
    {
        Pet* pet = GetPet();
        if (pet)
            pet->UpdateStats(stat);
    }

    switch (stat)
    {
        case STAT_AGILITY:
            UpdateAllCritPercentages();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            UpdateAllSpellCritChances();
            UpdateArmor();                                  //SPELL_AURA_MOD_RESISTANCE_OF_INTELLECT_PERCENT, only armor currently
            break;
        case STAT_SPIRIT:
            break;
        default:
            break;
    }

    if (stat == STAT_STRENGTH)
        UpdateAttackPowerAndDamage(false);
    else if (stat == STAT_AGILITY)
    {
        UpdateAttackPowerAndDamage(false);
        UpdateAttackPowerAndDamage(true);
    }

    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();

    // Update ratings in exist SPELL_AURA_MOD_RATING_FROM_STAT and only depends from stat
    uint32 mask = 0;
    AuraEffectList const& modRatingFromStat = GetAuraEffectsByType(SPELL_AURA_MOD_RATING_FROM_STAT);
    for (AuraEffectList::const_iterator i = modRatingFromStat.begin(); i != modRatingFromStat.end(); ++i)
        if (Stats((*i)->GetMiscValueB()) == stat)
            mask |= (*i)->GetMiscValue();
    if (mask)
    {
        for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
            if (mask & (1 << rating))
                ApplyRatingMod(CombatRating(rating), 0, true);
    }
    return true;
}

void Player::ApplySpellPowerBonus(int32 amount, bool apply)
{
    apply = _ModifyUInt32(apply, m_baseSpellPower, amount);

    // For speed just update for client
    ApplyModUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, amount, apply);
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, amount, apply);
}

void Player::UpdateSpellDamageAndHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Get healing bonus for all schools
    SetStatInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
    // Get damage bonus for all schools
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS+i, SpellBaseDamageBonusDone(SpellSchoolMask(1 << i)));
}

bool Player::UpdateAllStats()
{
    for (int8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        float value = GetTotalStatValue(Stats(i));
        SetStat(Stats(i), int32(value));
    }

    UpdateArmor();
    // calls UpdateAttackPowerAndDamage() in UpdateArmor for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
    UpdateAttackPowerAndDamage(true);
    UpdateMaxHealth();

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    // Custom MoP script
    // Jab Override Driver
    if (GetTypeId() == TYPEID_PLAYER && getClass() == CLASS_MONK)
    {
        Item* mainItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

        if (mainItem && mainItem->GetTemplate()->Class == ITEM_CLASS_WEAPON && !HasAura(125660))
        {
            RemoveAura(108561); // 2H Staff Override
            RemoveAura(115697); // 2H Polearm Override
            RemoveAura(115689); // D/W Axes
            RemoveAura(115694); // D/W Maces
            RemoveAura(115696); // D/W Swords

            switch (mainItem->GetTemplate()->SubClass)
            {
                case ITEM_SUBCLASS_WEAPON_STAFF:
                    CastSpell(this, 108561, true);
                    break;
                case ITEM_SUBCLASS_WEAPON_POLEARM:
                    CastSpell(this, 115697, true);
                    break;
                case ITEM_SUBCLASS_WEAPON_AXE:
                    CastSpell(this, 115689, true);
                    break;
                case ITEM_SUBCLASS_WEAPON_MACE:
                    CastSpell(this, 115694, true);
                    break;
                case ITEM_SUBCLASS_WEAPON_SWORD:
                    CastSpell(this, 115696, true);
                    break;
                default:
                    break;
            }
        }
        else if (HasAura(125660))
        {
            RemoveAura(108561); // 2H Staff Override
            RemoveAura(115697); // 2H Polearm Override
            RemoveAura(115689); // D/W Axes
            RemoveAura(115694); // D/W Maces
            RemoveAura(115696); // D/W Swords
        }
    }
    // Way of the Monk - 120277
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (getClass() == CLASS_MONK && HasAura(120277))
        {
            RemoveAurasDueToSpell(120275);
            RemoveAurasDueToSpell(108977);

            uint32 trigger = 0;
            if (IsTwoHandUsed())
            {
                trigger = 120275;
            }
            else
            {
                Item* mainItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                if (mainItem && mainItem->GetTemplate()->Class == ITEM_CLASS_WEAPON && offItem && offItem->GetTemplate()->Class == ITEM_CLASS_WEAPON)
                    trigger = 108977;
            }

            if (trigger)
                CastSpell(this, trigger, true);
        }
    }
    // Assassin's Resolve - 84601
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (getClass() == CLASS_ROGUE && ToPlayer()->GetSpecializationId(ToPlayer()->GetActiveSpec()) == SPEC_ROGUE_ASSASSINATION)
        {
            Item* mainItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

            if (((mainItem && mainItem->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER) || (offItem && offItem->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER)))
            {
                if (HasAura(84601))
                    RemoveAura(84601);

                CastSpell(this, 84601, true);
            }
            else
                RemoveAura(84601);
        }
    }

    UpdateAllRatings();
    UpdateAllCritPercentages();
    UpdateAllSpellCritChances();
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value  = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value));

        Pet* pet = GetPet();
        if (pet)
            pet->UpdateResistances(school);
    }
    else
        UpdateArmor();
}

void Player::UpdateArmor()
{
    float value = 0.0f;
    UnitMods unitMod = UNIT_MOD_ARMOR;

    value  = GetModifierValue(unitMod, BASE_VALUE);         // base armor (from items)
    value *= GetModifierValue(unitMod, BASE_PCT);           // armor percent from items
    value += GetModifierValue(unitMod, TOTAL_VALUE);

    // Custom MoP Script
    // 77494 - Mastery : Nature's Guardian
    if (GetTypeId() == TYPEID_PLAYER && HasAura(77494))
    {
        float Mastery = 1.0f + GetFloatValue(PLAYER_FIELD_MASTERY) * 1.25f / 100.0f;
        value *= Mastery;
    }

    //add dynamic flat mods
    AuraEffectList const& mResbyIntellect = GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
    for (AuraEffectList::const_iterator i = mResbyIntellect.begin(); i != mResbyIntellect.end(); ++i)
    {
        if ((*i)->GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL)
            value += CalculatePct(GetStat(Stats((*i)->GetMiscValueB())), (*i)->GetAmount());
    }

    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetArmor(int32(value));

    Pet* pet = GetPet();
    if (pet)
        pet->UpdateArmor();

    UpdateAttackPowerAndDamage();                           // armor dependent auras update for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
}

float Player::GetHealthBonusFromStamina()
{
    float ratio = 14.0f;
    GtOCTHpPerStaminaEntry const* hpBase = sGtOCTHpPerStaminaStore.LookupEntry(getLevel() - 1);
    if (hpBase)
        ratio = hpBase->ratio;

    // Taken from PaperDollFrame.lua 6.0 build 18888
    return GetStat(STAT_STAMINA) * ratio;
}

void Player::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + GetHealthBonusFromStamina();
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth((uint32)value);
}

void Player::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreatePowers(power);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE);
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    AuraEffectList const& mModMaxPower = GetAuraEffectsByType(SPELL_AURA_MOD_MAX_POWER);
    for (AuraEffectList::const_iterator i = mModMaxPower.begin(); i != mModMaxPower.end(); ++i)
        if (power == (*i)->GetMiscValue())
            value += float((*i)->GetAmount());

    value = floor(value + 0.5f);
    SetMaxPower(power, uint32(value));
}

void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;

    ChrClassesEntry const* entry = sChrClassesStore.LookupEntry(getClass());
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mod_pos = UNIT_FIELD_ATTACK_POWER_MOD_POS;
    uint16 index_mod_neg = UNIT_FIELD_ATTACK_POWER_MOD_NEG;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mod_pos = UNIT_FIELD_RANGED_ATTACK_POWER_MOD_POS;
        index_mod_neg = UNIT_FIELD_RANGED_ATTACK_POWER_MOD_NEG;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;
        val2 = GetStat(STAT_AGILITY) *  entry->RangedAttackPowerPerAgility;
    }
    else
    {
        float strengthValue = GetStat(STAT_STRENGTH) * entry->AttackPowerPerStrength;
        float agilityValue = GetStat(STAT_AGILITY) * entry->AttackPowerPerAgility;

        // Double bonus ??? Why so - THis should be here
        // if (GetShapeshiftForm() == FORM_CAT || GetShapeshiftForm() == FORM_BEAR)
        //     agilityValue += GetStat(STAT_AGILITY);

        val2 = strengthValue + agilityValue;
    }

    SetModifierValue(unitMod, BASE_VALUE, val2);

    float base_attPower = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMod = GetModifierValue(unitMod, TOTAL_VALUE);
    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    //add dynamic flat mods
    if (!ranged && HasAuraType(SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR))
    {
        AuraEffectList const& mAPbyArmor = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR);
        for (AuraEffectList::const_iterator iter = mAPbyArmor.begin(); iter != mAPbyArmor.end(); ++iter)
        {
            // always: ((*i)->GetModifier()->m_miscvalue == 1 == SPELL_SCHOOL_MASK_NORMAL)
            int32 temp = int32(GetArmor() / (*iter)->GetAmount());
            if (temp > 0)
                attPowerMod += temp;
            else
                attPowerMod -= temp;
        }
    }

    if (HasAuraType(SPELL_AURA_OVERRIDE_AP_BY_SPELL_POWER_PCT))
    {
        int32 ApBySpellPct = 0;
        int32 spellPower = ToPlayer()->GetBaseSpellPowerBonus(); // SpellPower from Weapon
        spellPower += std::max(0, int32(ToPlayer()->GetStat(STAT_INTELLECT)) - 10); // SpellPower from intellect

        AuraEffectList const& mAPFromSpellPowerPct = GetAuraEffectsByType(SPELL_AURA_OVERRIDE_AP_BY_SPELL_POWER_PCT);
        for (AuraEffectList::const_iterator i = mAPFromSpellPowerPct.begin(); i != mAPFromSpellPowerPct.end(); ++i)
            ApBySpellPct += CalculatePct(spellPower, (*i)->GetAmount());

        if (ApBySpellPct > 0)
        {
            SetInt32Value(index, uint32(ApBySpellPct));     //UNIT_FIELD_(RANGED)_ATTACK_POWER field
            SetFloatValue(index_mult, attPowerMultiplier);  //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field
        }
        else
        {
            SetInt32Value(index, uint32(base_attPower + attPowerMod));  //UNIT_FIELD_(RANGED)_ATTACK_POWER field
            SetFloatValue(index_mult, attPowerMultiplier);              //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field
        }
    }
    else
    {
        SetInt32Value(index, uint32(base_attPower + attPowerMod));  //UNIT_FIELD_(RANGED)_ATTACK_POWER field
        SetFloatValue(index_mult, attPowerMultiplier);              //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field
    }

    Pet* pet = GetPet();                                //update pet's AP
    //automatically update weapon damage after attack power modification
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);
        if (pet && pet->isHunterPet()) // At ranged attack change for hunter pet
            pet->UpdateAttackPowerAndDamage();
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (CanDualWield() && haveOffhandWeapon())           //allow update offhand damage only if player knows DualWield Spec and has equipped offhand weapon
            UpdateDamagePhysical(OFF_ATTACK);
        if (getClass() == CLASS_SHAMAN || getClass() == CLASS_PALADIN)                      // mental quickness
            UpdateSpellDamageAndHealingBonus();

        if (pet && pet->IsPetGhoul()) // At ranged attack change for hunter pet
            pet->UpdateAttackPowerAndDamage();
    }
}

void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& min_damage, float& max_damage)
{
    Item* mainItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    UnitMods unitMod;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    float att_speed = GetAPMultiplier(attType, normalized);
    float weapon_mindamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(attType, MAXDAMAGE);
    float attackPower = GetTotalAttackPowerValue(attType);


    float weapon_with_ap_min = (weapon_mindamage / att_speed) + (attackPower / 3.5f);
    float weapon_with_ap_max = (weapon_maxdamage / att_speed) + (attackPower / 3.5f);

    float weapon_normalized_min = weapon_with_ap_min * att_speed ;
    float weapon_normalized_max = weapon_with_ap_max * att_speed;

    if (IsInFeralForm())
    {
        float weaponSpeed = BASE_ATTACK_TIME / 1000.0f;
        if (mainItem && mainItem->GetTemplate()->Class == ITEM_CLASS_WEAPON)
            weaponSpeed = float(mainItem->GetTemplate()->Delay) / 1000.0f;

        if (GetShapeshiftForm() == FORM_CAT)
        {
            weapon_normalized_min = ((weapon_mindamage / weaponSpeed) + (attackPower / 3.5f));
            weapon_normalized_max = ((weapon_maxdamage / weaponSpeed) + (attackPower / 3.5f));
        }
        else if (GetShapeshiftForm() == FORM_BEAR)
        {
            weapon_normalized_min = ((weapon_mindamage / weaponSpeed) + (attackPower / 3.5f)) * 2.5f;
            weapon_normalized_max = ((weapon_maxdamage / weaponSpeed) + (attackPower / 3.5f)) * 2.5f;
        }
    }

    float base_value = GetModifierValue(unitMod, BASE_VALUE);
    float base_pct = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct = addTotalPct ? GetModifierValue(unitMod, TOTAL_PCT) : 1.0f;

    min_damage = ((base_value + weapon_normalized_min) * base_pct + total_value) * total_pct;
    max_damage = ((base_value + weapon_normalized_max) * base_pct + total_value) * total_pct;

    uint32 autoAttacksPctBonus = GetTotalAuraModifier(SPELL_AURA_MOD_AUTOATTACK_DAMAGE);
    AddPct(min_damage, autoAttacksPctBonus);
    AddPct(max_damage, autoAttacksPctBonus);
}

void Player::UpdateDamagePhysical(WeaponAttackType attType)
{
    float mindamage;
    float maxdamage;

    CalculateMinMaxDamage(attType, false, true, mindamage, maxdamage);

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UNIT_FIELD_MIN_DAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAX_DAMAGE, maxdamage);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MIN_OFF_HAND_DAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAX_OFF_HAND_DAMAGE, maxdamage);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MIN_RANGED_DAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAX_RANGED_DAMAGE, maxdamage);
            break;
    }
}

void Player::UpdateCritPercentage(WeaponAttackType attType)
{
    BaseModGroup modGroup;
    uint16 index;
    CombatRating cr;

    switch (attType)
    {
        case OFF_ATTACK:
            modGroup = OFFHAND_CRIT_PERCENTAGE;
            index = PLAYER_FIELD_OFFHAND_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            index = PLAYER_FIELD_RANGED_CRIT_PERCENTAGE;
            cr = CR_CRIT_RANGED;
            break;
        case BASE_ATTACK:
        default:
            modGroup = CRIT_PERCENTAGE;
            index = PLAYER_FIELD_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
    }

    float value = GetTotalPercentageModValue(modGroup) + GetRatingBonusValue(cr);
    // Modify crit from weapon skill and maximized defense skill of same level victim difference
    value += (int32(GetMaxSkillValueForLevel()) - int32(GetMaxSkillValueForLevel())) * 0.04f;
    if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
        value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_CRIT) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_CRIT) : value;

    value = value < 0.0f ? 0.0f : value;
    SetStatFloatValue(index, value);
}

void Player::UpdateAllCritPercentages()
{
    float crit = 0.0f;

    SetBaseModValue(CRIT_PERCENTAGE, PCT_MOD, crit);
    SetBaseModValue(OFFHAND_CRIT_PERCENTAGE, PCT_MOD, crit);
    SetBaseModValue(RANGED_CRIT_PERCENTAGE, PCT_MOD, crit);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

const float k_constant[MAX_CLASSES] =
{
    0.9560f,  // Warrior
    0.8860f,  // Paladin
    0.9880f,  // Hunter
    0.9880f,  // Rogue
    0.9830f,  // Priest
    0.9560f,  // DK
    0.9880f,  // Shaman
    0.9830f,  // Mage
    0.9830f,  // Warlock
    1.4220f,  // Monk
    1.2220f   // Druid
};

void Player::UpdateParryPercentage()
{
    const float parryCap[MAX_CLASSES] =
    {
        237.1860f,    // Warrior
        237.1860f,    // Paladin
        145.5604f,    // Hunter
        145.5604f,    // Rogue
        150.3759f,    // Priest
        237.1860f,    // DK
        145.5604f,    // Shaman
        150.3759f,    // Mage
        150.3759f,    // Warlock
        90.6425f,     // Monk
        150.3759f     // Druid
    };

    // No parry
    float value = 0.0f;
    uint32 pClass = getClass() - 1;

    if (CanParry())
    {
        float diminishing = 0.0f;
        float nondiminishing = 3.0f;

        // Parry from SPELL_AURA_MOD_PARRY_PERCENT aura
        nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);

        // Parry from rating
        diminishing = GetRatingBonusValue(CR_PARRY);

        // apply diminishing formula to diminishing parry chance
        value = nondiminishing + diminishing * parryCap[pClass] / (diminishing + parryCap[pClass] * k_constant[pClass]);
        if (value < 0.0f)
            value = 0.0f;

        if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
            value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_PARRY) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_PARRY) : value;
    }

    SetStatFloatValue(PLAYER_FIELD_PARRY_PERCENTAGE, value);
}

void Player::UpdateDodgePercentage()
{
    const float dodgeCap[MAX_CLASSES] =
    {
        90.6425f,     // Warrior
        66.5675f,     // Paladin
        145.5604f,    // Hunter
        145.5604f,    // Rogue
        150.3759f,    // Priest
        90.6425f,     // DK
        145.5604f,    // Shaman
        150.3759f,    // Mage
        150.3759f,    // Warlock
        501.2531f,    // Monk
        150.3759f     // Druid
    };

    float diminishing = 0.0f;
    float nondiminishing = 3.0f;

    // Dodge from SPELL_AURA_MOD_DODGE_PERCENT aura
    nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);

    // Dodge from rating
    diminishing += GetRatingBonusValue(CR_DODGE);

    uint32 pClass = getClass() - 1;

    // apply diminishing formula to diminishing dodge chance
    float value = nondiminishing + (diminishing * dodgeCap[pClass] / (diminishing + dodgeCap[pClass] * k_constant[pClass]));
    if (value < 0.0f)
        value = 0.0f;

    if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
        value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_DODGE) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_DODGE) : value;

    SetStatFloatValue(PLAYER_FIELD_DODGE_PERCENTAGE, value);
}

void Player::UpdateBlockPercentage()
{
    const float blockCap[MAX_CLASSES] =
    {
        90.6425f,     // Warrior
        66.5675f,     // Paladin
        145.5604f,    // Hunter
        145.5604f,    // Rogue
        150.3759f,    // Priest
        90.6425f,     // DK
        145.5604f,    // Shaman
        150.3759f,    // Mage
        150.3759f,    // Warlock
        501.2531f,    // Monk
        150.3759f     // Druid
    };

    // No block
    float value = 0.0f;
    uint32 pClass = getClass() - 1;

    if (CanBlock())
    {
        float diminishing = 0.0f;
        float nondiminishing = 3.0f;

        // Dodge from SPELL_AURA_MOD_BLOCK_PERCENT aura
        nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);

        // Dodge from rating
        diminishing += GetRatingBonusValue(CR_BLOCK);

        // apply diminishing formula to diminishing dodge chance
        value = nondiminishing + (diminishing * blockCap[pClass] / (diminishing + blockCap[pClass] * k_constant[pClass]));

        // Custom MoP Script
        // 76671 - Mastery : Divine Bulwark - Block Percentage
        if (GetTypeId() == TYPEID_PLAYER && HasAura(76671))
            value += GetFloatValue(PLAYER_FIELD_MASTERY);

        // Custom MoP Script
        // 76857 - Mastery : Critical Block - Block Percentage
        if (GetTypeId() == TYPEID_PLAYER && HasAura(76857))
            value += GetFloatValue(PLAYER_FIELD_MASTERY) / 2.0f;

        if (value < 0.0f)
            value = 0.0f;

        if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
            value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_BLOCK) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_BLOCK) : value;
    }

    SetStatFloatValue(PLAYER_FIELD_BLOCK_PERCENTAGE, value);
}

void Player::UpdateSpellCritChance(uint32 school)
{
    // For normal school set zero crit chance
    if (school == SPELL_SCHOOL_NORMAL)
    {
        SetFloatValue(PLAYER_FIELD_SPELL_CRIT_PERCENTAGE, 0.0f);
        return;
    }
    // For others recalculate it from:
    float crit = 0.0f;
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit from SPELL_AURA_MOD_CRIT_PCT
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PCT);
    // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    crit += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1<<school);
    // Increase crit from spell crit ratings
    crit += GetRatingBonusValue(CR_CRIT_SPELL);

    // Store crit value
    SetFloatValue(PLAYER_FIELD_SPELL_CRIT_PERCENTAGE + school, crit);

    if (Pet* pet = GetPet())
        pet->m_baseSpellCritChance = crit;
}

void Player::UpdateMasteryPercentage()
{
    // No mastery
    float value = 0.0f;
    if (CanMastery() && getLevel() >= 80)
    {
        // Mastery from SPELL_AURA_MASTERY aura
        value += GetTotalAuraModifier(SPELL_AURA_MASTERY);
        // Mastery from rating
        value += GetRatingBonusValue(CR_MASTERY);
        value = value < 0.0f ? 0.0f : value;
    }
    SetFloatValue(PLAYER_FIELD_MASTERY, value);
    // Custom MoP Script
    // 76671 - Mastery : Divine Bulwark - Update Block Percentage
    // 76857 - Mastery : Critical Block - Update Block Percentage
    if (HasAura(76671) || HasAura(76857))
        UpdateBlockPercentage();
    // 77494 - Mastery : Nature's Guardian - Update Armor
    if (HasAura(77494))
        UpdateArmor();
}

void Player::UpdatePvPPowerPercentage()
{
    float value = GetRatingBonusValue(CR_PVP_POWER);
    value = value < 0.0f ? 0.0f : value;

    float damage_value = value;
    float heal_value = value;

    switch (GetSpecializationId(GetActiveSpec()))
    {
        // All other specializations and classes (including tanking) receive a 40% bonus to healing from PvP Power.
        case SPEC_WARRIOR_ARMS:
        case SPEC_WARRIOR_FURY:
        case SPEC_WARRIOR_PROTECTION:
        case SPEC_HUNTER_BEASTMASTER:
        case SPEC_HUNTER_MARKSMAN:
        case SPEC_HUNTER_SURVIVAL:
        case SPEC_ROGUE_ASSASSINATION:
        case SPEC_ROGUE_COMBAT:
        case SPEC_ROGUE_SUBTLETY:
        case SPEC_DK_BLOOD:
        case SPEC_DK_FROST:
        case SPEC_DK_UNHOLY:
        case SPEC_MAGE_ARCANE:
        case SPEC_MAGE_FIRE:
        case SPEC_MAGE_FROST:
        case SPEC_WARLOCK_AFFLICTION:
        case SPEC_WARLOCK_DEMONOLOGY:
        case SPEC_WARLOCK_DESTRUCTION:
            heal_value *= 0.4f;
            break;
        // Healing specializations receive a 100% bonus to healing from PvP Power.
        case SPEC_PALADIN_HOLY:
        case SPEC_PRIEST_DISCIPLINE:
        case SPEC_PRIEST_HOLY:
        case SPEC_SHAMAN_RESTORATION:
        case SPEC_DROOD_RESTORATION:
        case SPEC_MONK_MISTWEAVER:
            damage_value = 0.0f;
            break;
        // Damage specializations for Druids, Monks, Paladins, Priests, and Shaman receive a 70% bonus to healing from PvP Power.
        default:
            heal_value *= 0.7f;
            break;
    }

    SetFloatValue(PLAYER_FIELD_PVP_POWER_DAMAGE, damage_value);
    SetFloatValue(PLAYER_FIELD_PVP_POWER_HEALING, heal_value);
}

void Player::UpdateAllSpellCritChances()
{
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateSpellCritChance(i);
}

void Player::ApplyManaRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseManaRegen, amount);
    UpdateManaRegen();
}

void Player::ApplyHealthRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseHealthRegen, amount);
}

void Player::UpdateManaRegen()
{
    if (getPowerType() != POWER_MANA && !IsInFeralForm())
        return;

    uint32 hp;
    uint32 mana = 0;
    float spiritRegen = OCTRegenMPPerSpirit();
    float outOfCombatRegen;
    float combatRegen;

    sObjectMgr->GetPlayerClassLevelInfo(getClass(), getLevel(), hp, mana);
    mana *= GetTotalAuraMultiplier(SPELL_AURA_MODIFY_MANA_REGEN_FROM_MANA_PCT);

    if (GetRoleForGroup() != ROLE_HEALER)
    {
        combatRegen = 0.004f * mana + (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) / 5.0f);
        outOfCombatRegen = 0.004f * mana + spiritRegen + (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) / 5.0f);
    }
    else
    {
        combatRegen = spiritRegen + (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) / 5.0f);
        outOfCombatRegen = 0.004f * mana + spiritRegen + (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) / 5.0f);
    }

    if (HasAuraType(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT))
        combatRegen *= 1.5f; // Allows 50% of your mana regeneration from Spirit to continue while in combat.

    outOfCombatRegen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);
    combatRegen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

    // Mana Meditation && Meditation

    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER, combatRegen);
    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER, outOfCombatRegen);
}

void Player::UpdateEnergyRegen()
{
    if (getPowerType() != POWER_ENERGY)
        return;

    float pct = 0.0f;
    Unit::AuraEffectList const& ModPowerRegenPCTAuras = GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    for (Unit::AuraEffectList::const_iterator i = ModPowerRegenPCTAuras.begin(); i != ModPowerRegenPCTAuras.end(); ++i)
    if (Powers((*i)->GetMiscValue()) == POWER_ENERGY)
        pct += (*i)->GetAmount();

    float haste = 1.f / (1.f + (m_baseRatingValue[CR_HASTE_MELEE] * GetRatingMultiplier(CR_HASTE_MELEE) + pct) / 100.f);

    SetFloatValue(UNIT_FIELD_MOD_HASTE_REGEN, haste);
}

void Player::UpdateRuneRegen(RuneType rune)
{
    if (rune > NUM_RUNE_TYPES)
        return;

    uint32 cooldown = 0;

    for (uint32 i = 0; i < MAX_RUNES; ++i)
        if (GetBaseRune(i) == rune)
        {
            cooldown = GetRuneBaseCooldown(i);
            break;
        }

    if (cooldown <= 0)
        return;

    float regen = float(1 * IN_MILLISECONDS) / float(cooldown);
    SetFloatValue(PLAYER_FIELD_RUNE_REGEN + uint8(rune), regen);
}

void Player::UpdateAllRunesRegen()
{
    for (uint8 i = 0; i < NUM_RUNE_TYPES; ++i)
    {
        if (uint32 cooldown = GetRuneTypeBaseCooldown(RuneType(i)))
        {
            float regen = float(1 * IN_MILLISECONDS) / float(cooldown);

            if (regen < 0.0099999998f)
                regen = 0.01f;

            SetFloatValue(PLAYER_FIELD_RUNE_REGEN + i, regen);
        }
    }

    float pct = 0.f;
    AuraEffectList const& regenAura = GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    for (AuraEffectList::const_iterator i = regenAura.begin(); i != regenAura.end(); ++i)
        if ((*i)->GetMiscValue() == POWER_RUNES)
            pct += (*i)->GetAmount();

    float haste = 1.f / (1.f + (m_baseRatingValue[CR_HASTE_MELEE] * GetRatingMultiplier(CR_HASTE_MELEE) + pct) / 100.f);
    SetFloatValue(UNIT_FIELD_MOD_HASTE_REGEN, haste);
}

void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraStatMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraStatMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Creature::UpdateStats(Stats /*stat*/)
{
    return true;
}

bool Creature::UpdateAllStats()
{
    UpdateMaxHealth();
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    for (int8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

void Creature::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value  = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Creature::UpdateArmor()
{
    float value = GetTotalAuraModValue(UNIT_MOD_ARMOR);
    SetArmor(int32(value));
}

void Creature::UpdateMaxHealth()
{
    float value = GetTotalAuraModValue(UNIT_MOD_HEALTH);
    SetMaxHealth((uint32)value);
}

void Creature::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float value  = GetTotalAuraModValue(unitMod);
    SetMaxPower(power, uint32(value));
}

void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;
    }

    float base_attPower  = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    SetInt32Value(index, (uint32)base_attPower);            //UNIT_FIELD_(RANGED)_ATTACK_POWER field
    SetFloatValue(index_mult, attPowerMultiplier);          //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field

    //automatically update weapon damage after attack power modification
    if (ranged)
        UpdateDamagePhysical(RANGED_ATTACK);
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        UpdateDamagePhysical(OFF_ATTACK);
    }
}

void Creature::UpdateDamagePhysical(WeaponAttackType p_AttType)
{
    float l_Variance = 1.f;
    UnitMods l_UnitMod;
    switch (p_AttType)
    {
        case BASE_ATTACK:
        default:
            l_UnitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            l_UnitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            l_UnitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    float l_WeaponMinDamage = GetWeaponDamageRange(p_AttType, MINDAMAGE);
    float l_WeaponMaxDamage = GetWeaponDamageRange(p_AttType, MAXDAMAGE);

    /* difference in AP between current attack power and base value from DB */
    // creature's damage in battleground is calculated in Creature::SelectLevel
    // so don't change it with ap and dmg multipliers

    float l_AttackPower      = GetTotalAttackPowerValue(p_AttType);
    float l_AttackSpeedMulti = GetAPMultiplier(p_AttType, false);
    float l_BaseValue        = GetModifierValue(l_UnitMod, BASE_VALUE) + (l_AttackPower / 14.0f) * l_Variance;
    float l_BasePct          = GetModifierValue(l_UnitMod, BASE_PCT) * l_AttackSpeedMulti;
    float l_TotalValue       = GetModifierValue(l_UnitMod, TOTAL_VALUE);
    float l_TotalPct         = GetModifierValue(l_UnitMod, TOTAL_PCT);
    float l_DmgMultiplier    = GetCreatureTemplate()->dmg_multiplier;

    if (!CanUseAttackType(p_AttType))
    {
        l_WeaponMinDamage = 0;
        l_WeaponMaxDamage = 0;
    }

    float mindamage = ((l_BaseValue + l_WeaponMinDamage) * l_DmgMultiplier * l_BasePct + l_TotalValue) * l_TotalPct;
    float maxdamage = ((l_BaseValue + l_WeaponMaxDamage) * l_DmgMultiplier * l_BasePct + l_TotalValue) * l_TotalPct;

    switch (p_AttType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UNIT_FIELD_MIN_DAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAX_DAMAGE, maxdamage);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MIN_OFF_HAND_DAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAX_OFF_HAND_DAMAGE, maxdamage);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MIN_RANGED_DAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAX_RANGED_DAMAGE, maxdamage);
            break;
    }
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Guardian::UpdateStats(Stats stat)
{
    if (stat >= MAX_STATS)
        return false;

    float value  = GetTotalStatValue(stat);
    ApplyStatBuffMod(stat, m_statFromOwner[stat], false);
    float ownersBonus = 0.0f;

    Unit* owner = GetOwner();
    // Handle Death Knight Glyphs and Talents
    float mod = 0.75f;

    switch (stat)
    {
        case STAT_STAMINA:
        {
            mod = 0.3f;

            if (IsPetGhoul() || IsPetGargoyle())
                mod = 0.45f;
            else if (owner->getClass() == CLASS_WARLOCK && isPet())
                mod = 0.75f;
            else if (owner->getClass() == CLASS_MAGE && isPet())
                mod = 0.75f;
            else
            {
                mod = 0.45f;

                if (isPet())
                {
                    switch (ToPet()->GetSpecializationId())
                    {
                        case SPEC_PET_FEROCITY: mod = 0.67f; break;
                        case SPEC_PET_TENACITY: mod = 0.78f; break;
                        case SPEC_PET_CUNNING: mod = 0.725f; break;
                    }
                }
            }

            ownersBonus = float(owner->GetStat(stat)) * mod;
            ownersBonus *= GetModifierValue(UNIT_MOD_STAT_STAMINA, TOTAL_PCT);

            float modifier = 1.0f;
            AuraEffectList const& mModHealthFromOwner = owner->GetAuraEffectsByType(SPELL_AURA_INCREASE_HEALTH_FROM_OWNER);
            for (AuraEffectList::const_iterator i = mModHealthFromOwner.begin(); i != mModHealthFromOwner.end(); ++i)
                modifier += float((*i)->GetAmount() / 100.0f);

            ownersBonus *= modifier;
            value += ownersBonus;
            break;
        }
        case STAT_STRENGTH:
        {
            mod = 0.7f;

            ownersBonus = owner->GetStat(stat) * mod;
            value += ownersBonus;
            break;
        }
        case STAT_INTELLECT:
        {
            if (owner->getClass() == CLASS_WARLOCK || owner->getClass() == CLASS_MAGE)
            {
                mod = 0.3f;
                ownersBonus = owner->GetStat(stat) * mod;
            }
            else if (owner->getClass() == CLASS_DEATH_KNIGHT && GetEntry() == 31216)
            {
                mod = 0.3f;
                if (owner->GetSimulacrumTarget())
                    ownersBonus = owner->GetSimulacrumTarget()->GetStat(stat) * mod;
                else
                    ownersBonus = owner->GetStat(stat) * mod;
            }

            value += ownersBonus;
            break;
        }
        default:
            break;
    }

    SetStat(stat, int32(value));
    m_statFromOwner[stat] = ownersBonus;
    ApplyStatBuffMod(stat, m_statFromOwner[stat], true);

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateAttackPowerAndDamage();
            break;
        case STAT_AGILITY:
            UpdateArmor();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            if (owner->getClass() == CLASS_MAGE)
                UpdateAttackPowerAndDamage();
            break;
        case STAT_SPIRIT:
        default:
            break;
    }

    return true;
}

bool Guardian::UpdateAllStats()
{
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        UpdateStats(Stats(i));

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

void Guardian::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value  = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));

        // hunter and warlock pets gain 40% of owner's resistance
        if (isPet())
            value += float(CalculatePct(m_owner->GetResistance(SpellSchools(school)), 40));

        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Guardian::UpdateArmor()
{
    float value = 0.0f;
    UnitMods unitMod = UNIT_MOD_ARMOR;

    // All pets gain 100% of owner's armor value
    value = m_owner->GetArmor();

    switch (GetEntry())
    {
        case ENTRY_IMP:
        case ENTRY_FEL_IMP:
        case ENTRY_FELHUNTER:
        case ENTRY_OBSERVER:
        case ENTRY_SUCCUBUS:
        case ENTRY_SHIVARRA:
            value *= 3;
            break;
        case ENTRY_VOIDWALKER:
        case ENTRY_VOIDLORD:
            value *= 4;
            break;
        default:
            break;
    }

    value *= GetModifierValue(unitMod, BASE_PCT);
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    if (Unit* owner = GetOwner())
    {
        AuraEffectList const& mModPetStats = owner->GetAuraEffectsByType(SPELL_AURA_MOD_PET_STATS);
        float amount = 0;
        for (AuraEffectList::const_iterator i = mModPetStats.begin(); i != mModPetStats.end(); ++i)
            if ((*i)->GetMiscValue() == INCREASE_ARMOR_PERCENT && (*i)->GetMiscValueB() && GetEntry() == (*i)->GetMiscValueB())
                amount += float((*i)->GetAmount());

        AddPct(value, amount);
    }

    SetArmor(int32(value));
}

void Guardian::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;
    float stamina = GetStat(STAT_STAMINA) - GetCreateStat(STAT_STAMINA);

    float multiplicator;
    switch (GetEntry())
    {
        case ENTRY_GARGOYLE:
            multiplicator = 15.0f;
            break;
        case ENTRY_WATER_ELEMENTAL:
            multiplicator = 1.0f;
            stamina = 0.0f;
            break;
        case ENTRY_BLOODWORM:
            SetMaxHealth(GetCreateHealth());
            return;
        default:
            multiplicator = 10.0f;
            break;
    }

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + stamina * multiplicator;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    Unit* owner = GetOwner();
    switch (GetEntry())
    {
        case ENTRY_IMP:
        case ENTRY_FEL_IMP:
            value = owner->CountPctFromMaxHealth(30);
            break;
        case ENTRY_FELHUNTER:
        case ENTRY_OBSERVER:
        case ENTRY_SUCCUBUS:
        case ENTRY_SHIVARRA:
            value = owner->CountPctFromMaxHealth(50);
            break;
        case ENTRY_VOIDWALKER:
        case ENTRY_FELGUARD:
        case ENTRY_GHOUL:
            value = owner->CountPctFromMaxHealth(50);
            break;
        case ENTRY_WRATHGUARD:
        case ENTRY_VOIDLORD:
            value = owner->CountPctFromMaxHealth(60);
            break;
        default:
            break;
    }

    if (Unit* owner = GetOwner())
    {
        AuraEffectList const& mModPetStats = owner->GetAuraEffectsByType(SPELL_AURA_MOD_PET_STATS);
        float amount = 0;
        for (AuraEffectList::const_iterator i = mModPetStats.begin(); i != mModPetStats.end(); ++i)
            if ((*i)->GetMiscValue() == INCREASE_HEALTH_PERCENT && (*i)->GetMiscValueB() && GetEntry() == (*i)->GetMiscValueB())
                amount += float((*i)->GetAmount());

        AddPct(value, amount);
    }

    SetMaxHealth((uint32)value);
}

void Guardian::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float addValue = (power == POWER_MANA) ? GetStat(STAT_INTELLECT) - GetCreateStat(STAT_INTELLECT) : 0.0f;
    float multiplicator = 15.0f;

    switch (GetEntry())
    {
        case ENTRY_IMP:
            multiplicator = 4.95f;
            break;
        case ENTRY_VOIDWALKER:
        case ENTRY_SUCCUBUS:
        case ENTRY_FELHUNTER:
        case ENTRY_FELGUARD:
            multiplicator = 11.5f;
            break;
        case ENTRY_WATER_ELEMENTAL:
            multiplicator = 1.0f;
            addValue = 0.0f;
            break;
        default:
            multiplicator = 15.0f;
            break;
    }

    float value  = GetModifierValue(unitMod, BASE_VALUE) + GetCreatePowers(power);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + addValue * multiplicator;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, uint32(value));
}

void Guardian::UpdateAttackPowerAndDamage(bool ranged)
{
    if (ranged)
        return;

    float val = 0.0f;
    float bonusAP = 0.0f;
    UnitMods unitMod = UNIT_MOD_ATTACK_POWER;

    // imp's attack power
    if (GetEntry() == ENTRY_IMP)
        val = GetStat(STAT_STRENGTH) - 10.0f;
    else if (!isHunterPet())
        val = 2 * GetStat(STAT_STRENGTH) - 20.0f;

    Unit* owner = GetOwner();
    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
    {
        if (isHunterPet())
        {
            bonusAP = owner->GetTotalAttackPowerValue(RANGED_ATTACK); // Hunter pets gain 100% of owner's AP
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.5f)); // Bonus damage is equal to 50% of owner's AP
        }
        else if (IsPetGhoul()) // ghouls benefit from deathknight's attack power (may be summon pet or not)
        {
            bonusAP = owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.22f;
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.1287f));
        }
        else if (isPet() && GetEntry() != ENTRY_WATER_ELEMENTAL || IsTreant()) // demons benefit from warlocks shadow or fire damage
        {
            int32 spd = owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL);
            SetBonusDamage(spd);
            bonusAP = spd;
        }
        else if (GetEntry() == ENTRY_WATER_ELEMENTAL) // water elementals benefit from mage's frost damage
        {
            SetBonusDamage(int32(m_owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FROST)));
        }
        // Summon Gargoyle AP
        else if (GetEntry() == 27829)
        {
            bonusAP = owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.4f;
            SetBonusDamage(int32(m_owner->GetTotalAttackPowerValue(BASE_ATTACK)));
        }
    }

    SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, val + bonusAP);

    //in BASE_VALUE of UNIT_MOD_ATTACK_POWER for creatures we store data of meleeattackpower field in DB
    float base_attPower = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    // base attackPower for Warlock pets
    if (owner && owner->getClass() == CLASS_WARLOCK && owner->ToPlayer())
    {
        switch (GetEntry())
        {
            case ENTRY_IMP:
            case ENTRY_FEL_IMP:
            case ENTRY_FELHUNTER:
            case ENTRY_OBSERVER:
            case ENTRY_VOIDWALKER:
            case ENTRY_VOIDLORD:
            case ENTRY_FELGUARD:
                base_attPower = owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 3.5f;
                break;
            case ENTRY_SUCCUBUS:
                base_attPower = owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 1.67f;
                break;
            case ENTRY_SHIVARRA:
                base_attPower = owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 1.11f;
                break;
            case ENTRY_WRATHGUARD:
                base_attPower = owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 2.33f;
                break;
            default:
                break;
        }
    }

    //UNIT_FIELD_(RANGED)_ATTACK_POWER field
    SetInt32Value(UNIT_FIELD_ATTACK_POWER, (int32)base_attPower);
    //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, attPowerMultiplier);

    //automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
}

void Guardian::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK)
        return;

    float bonusDamage = 0.0f;
    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        if (GetEntry() == ENTRY_FIRE_ELEMENTAL) // greater fire elemental
        {
            int32 spellDmg = int32(m_owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE)) - m_owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + SPELL_SCHOOL_FIRE);
            if (spellDmg > 0)
                bonusDamage = spellDmg * 0.4f;
        }
    }

    UnitMods unitMod = UNIT_MOD_DAMAGE_MAINHAND;

    float att_speed = float(GetAttackTime(BASE_ATTACK))/1000.0f;

    float base_value  = GetModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType)/ 14.0f * att_speed  + bonusDamage;
    float base_pct    = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);

    // Min and Max damage for Warlock pets
    if (m_owner && m_owner->ToPlayer() && m_owner->getClass() == CLASS_WARLOCK)
    {
        switch (GetEntry())
        {
            case ENTRY_IMP:
            case ENTRY_FELHUNTER:
            case ENTRY_VOIDWALKER:
            case ENTRY_FELGUARD:
                base_value = 1068 + (m_owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 3.5f) / 14.0f * 2 + bonusDamage;
                break;
            case ENTRY_SUCCUBUS:
                base_value = 1068 + (m_owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 1.67f) / 14.0f * 3 + bonusDamage;
                break;
            case ENTRY_FEL_IMP:
            case ENTRY_VOIDLORD:
            case ENTRY_OBSERVER:
                base_value = (1068 + (m_owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 3.5f) / 14.0f * 2 + bonusDamage) * 1.2f;
                break;
            case ENTRY_SHIVARRA:
                base_value = (713 + (m_owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 3.5f) / 14.0f * 3 + bonusDamage) * 1.2f;
                break;
            case ENTRY_WRATHGUARD:
                base_value = (713 + (m_owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL) * 2.33f) / 14.0f * 2 + bonusDamage) * 1.2f;
                break;
            default:
                break;
        }

        if (m_owner->HasAura(77219))
        {
            float Mastery = 1.f + m_owner->GetFloatValue(PLAYER_FIELD_MASTERY) / 100.f;
            base_value *= Mastery;
        }
    }

    float mindamage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    float maxdamage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;

    SetStatFloatValue(UNIT_FIELD_MIN_DAMAGE, mindamage);
    SetStatFloatValue(UNIT_FIELD_MAX_DAMAGE, maxdamage);

    if (GetEntry() == ENTRY_WRATHGUARD || GetEntry() == ENTRY_SHIVARRA)
    {
        SetStatFloatValue(UNIT_FIELD_MIN_OFF_HAND_DAMAGE, mindamage / 2);
        SetStatFloatValue(UNIT_FIELD_MIN_OFF_HAND_DAMAGE, maxdamage / 2);
    }
}

void Guardian::SetBonusDamage(int32 damage)
{
    m_bonusSpellDamage = damage;
    if (GetOwner()->GetTypeId() == TYPEID_PLAYER)
        GetOwner()->SetUInt32Value(PLAYER_FIELD_PET_SPELL_POWER, damage);
}

void Player::UpdateMultistrike()
{
    float value = GetTotalAuraModifier(SPELL_AURA_MOD_MULTISTRIKE_PCT);
    float effect = 30.f; // Default value
    effect += GetTotalAuraModifier(SPELL_AURA_MOD_MULTISTRIKE_EFFECT_PCT);
    value += GetRatingBonusValue(CR_MULTISTRIKE);
    SetFloatValue(PLAYER_FIELD_MULTISTRIKE, value);
    SetFloatValue(PLAYER_FIELD_MULTISTRIKE_EFFECT, effect / 100.f);
}

void Player::UpdateLeech()
{
    float value = 0.f;//GetTotalAuraModifier(SPELL_AURA);
    value += GetRatingBonusValue(CR_LIFESTEAL);
    SetFloatValue(PLAYER_FIELD_LIFESTEAL, value);
}

void Player::UpdateVesatillity()
{
    float valueDone = 0.f;//GetTotalAuraModifier(SPELL_AURA);
    float valueTaken = 0.f;//GetTotalAuraModifier(SPELL_AURA);
    valueDone += GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE);
    valueTaken += GetRatingBonusValue(CR_VERSATILITY_DAMAGE_TAKEN);
    SetFloatValue(PLAYER_FIELD_VERSATILITY, valueDone);
    SetFloatValue(PLAYER_FIELD_VERSATILITY_BONUS, valueTaken);
}

void Player::UpdateAvoidance()
{
    float value = 0.f;//GetTotalAuraModifier(SPELL_AURA);
    value += GetRatingBonusValue(CR_AVOIDANCE);
    SetFloatValue(PLAYER_FIELD_AVOIDANCE, value);
}
