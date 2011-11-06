using System;
using WowPacketParser.Enums;
using WowPacketParser.Misc;

namespace WowPacketParser.Parsing.Parsers
{
    public static class InstanceHandler
    {
        [Parser(Opcode.MSG_SET_DUNGEON_DIFFICULTY)]
        [Parser(Opcode.MSG_SET_RAID_DIFFICULTY)]
        public static void HandleSetDifficulty(Packet packet)
        {
            packet.ReadEnum<MapDifficulty>("Difficulty", TypeCode.Int32);
            if (packet.Direction != Direction.ServerToClient)
                return;

            packet.ReadInt32("Unk Int32");
            packet.ReadInt32("In Group");
        }

        [Parser(Opcode.SMSG_INSTANCE_DIFFICULTY)]
        public static void HandleInstanceDifficulty(Packet packet)
        {
            packet.ReadEnum<MapDifficulty>("Difficulty", TypeCode.Int32);
            packet.ReadInt32("Player Difficulty");
        }

        [Parser(Opcode.SMSG_CHANGEPLAYER_DIFFICULTY_RESULT)]
        public static void HandlePlayerChangeDifficulty(Packet packet)
        {
            var type = packet.ReadEnum<DifficultyChangeType>("Change Type", TypeCode.Int32);
            switch (type)
            {
                case DifficultyChangeType.PlayerDifficulty1:
                    packet.ReadByte("Player Difficulty");
                    break;
                case DifficultyChangeType.SpellDuration:
                    packet.ReadInt32("Spell Duration");
                    break;
                case DifficultyChangeType.Time:
                    packet.ReadInt32("Time");
                    break;
                case DifficultyChangeType.MapDifficulty:
                    packet.ReadEnum<MapDifficulty>("Difficulty", TypeCode.Int32);
                    break;
            }
        }

        [Parser(Opcode.SMSG_UPDATE_LAST_INSTANCE)]
        [Parser(Opcode.SMSG_RESET_FAILED_NOTIFY)]
        public static void HandleResetFailedNotify(Packet packet)
        {
            packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Map Id");
        }

        [Parser(Opcode.MSG_RAID_TARGET_UPDATE)]
        public static void HandleRaidTargetUpdate(Packet packet)
        {
            var type = packet.ReadByte("Type");
            if (type == 0)
            {
                packet.ReadGuid("Who GUID");
                packet.ReadByte("Icon Id");
                packet.ReadGuid("Target GUID");
            }
            else if (type != 255 && packet.Direction == Direction.ClientToServer)
                packet.ReadGuid("Target GUID");
        }

        [Parser(Opcode.SMSG_RAID_INSTANCE_MESSAGE)]
        public static void HandleRaidInstanceMessage(Packet packet)
        {
            var type = packet.ReadEnum<RaidInstanceResetWarning>("Warning Type", TypeCode.Int32);
            packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Map Id");
            packet.ReadEnum<MapDifficulty>("Difficulty", TypeCode.Int32);
            packet.ReadInt32("Reset time");
            if (type == RaidInstanceResetWarning.Welcome)
            {
                packet.ReadBoolean("Unk bool");
                packet.ReadBoolean("Is Extended");
            }
        }

        [Parser(Opcode.SMSG_UPDATE_INSTANCE_OWNERSHIP)]
        [Parser(Opcode.SMSG_INSTANCE_SAVE_CREATED)]
        public static void HandleUpdateInstanceOwnership(Packet packet)
        {
            packet.ReadInt32("Unk");
        }

        [Parser(Opcode.SMSG_RAID_INSTANCE_INFO)]
        public static void HandleRaidInstanceInfo(Packet packet)
        {
            var counter = packet.ReadInt32("Counter");
            for (var i = 0; i < counter; ++i)
            {
                packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Map ID", i);
                packet.ReadEnum<MapDifficulty>("Map Difficulty", TypeCode.UInt32, i);
                packet.ReadGuid("Instance GUID", i);
                packet.ReadBoolean("Expired", i);
                packet.ReadBoolean("Extended", i);
                packet.ReadUInt32("Reset Time", i);
            }
        }

    }
}
