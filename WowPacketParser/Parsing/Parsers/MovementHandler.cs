using System;
using System.Collections.Generic;
using WowPacketParser.Enums;
using WowPacketParser.Enums.Version;
using WowPacketParser.Misc;
using WowPacketParser.Misc.Objects;
using Guid = WowPacketParser.Misc.Guid;

namespace WowPacketParser.Parsing.Parsers
{
    public static class MovementHandler
    {
        public static Vector4 CurrentPosition;

        public static uint CurrentMapId;

        public static int CurrentPhaseMask = 1;

        public static MovementInfo ReadMovementInfo(Packet packet, Guid guid)
        {
            return ReadMovementInfo(packet, guid, -1);
        }

        public static MovementInfo ReadMovementInfo(Packet packet, Guid guid, int index)
        {
            string prefix = index < 0 ? string.Empty : "[" + index + "] ";

            var info = new MovementInfo();
            info.Flags = packet.ReadEnum<MovementFlag>(prefix + "Movement Flags", TypeCode.Int32);

            var flagsTypeCode = ClientVersion.AddedInVersion(ClientVersionBuild.V3_0_2_9056) ? TypeCode.Int16 : TypeCode.Byte;
            var flags = packet.ReadEnum<MovementFlagExtra>(prefix + "Extra Movement Flags", flagsTypeCode);

            if (ClientVersion.AddedInVersion(ClientVersionBuild.V4_2_2_14545))
                if (packet.ReadGuid(prefix + "GUID 2") != guid)
                    Console.WriteLine("GUIDS NOT EQUAL"); // Fo debuggingz

            packet.ReadInt32(prefix + "Time");

            var pos = packet.ReadVector4(prefix + "Position");
            info.Position = new Vector3(pos.X, pos.Y, pos.Z);
            info.Orientation = pos.O;

            if (info.Flags.HasAnyFlag(MovementFlag.OnTransport))
            {
                if (ClientVersion.AddedInVersion(ClientVersionBuild.V3_1_0_9767))
                    packet.ReadPackedGuid(prefix + "Transport GUID");
                else
                    packet.ReadGuid(prefix + "Transport GUID");

                packet.ReadVector4(prefix + "Transport Position");
                packet.ReadInt32(prefix + "Transport Time");

                if (ClientVersion.AddedInVersion(ClientType.WrathOfTheLichKing))
                    packet.ReadByte(prefix + "Transport Seat");

                if (flags.HasAnyFlag(MovementFlagExtra.InterpolateMove))
                    packet.ReadInt32(prefix + "Transport Time");
            }

            if (info.Flags.HasAnyFlag(MovementFlag.Swimming | MovementFlag.Flying) ||
                flags.HasAnyFlag(MovementFlagExtra.AlwaysAllowPitching))
                packet.ReadSingle(prefix + "Swim Pitch");

            if (ClientVersion.RemovedInVersion(ClientType.Cataclysm))
                packet.ReadInt32(prefix + "Fall Time");

            if (info.Flags.HasAnyFlag(MovementFlag.Falling))
            {
                if (ClientVersion.AddedInVersion(ClientType.Cataclysm))
                    packet.ReadInt32(prefix + "Fall Time");

                packet.ReadSingle(prefix + "Fall Velocity");
                packet.ReadSingle(prefix + "Fall Sin angle");
                packet.ReadSingle(prefix + "Fall Cos angle");
                packet.ReadSingle(prefix + "Fall Speed");
            }

            if (info.Flags.HasAnyFlag(MovementFlag.SplineElevation))
                packet.ReadSingle(prefix + "Spline Elevation");

            return info;
        }

        [Parser(Opcode.SMSG_MONSTER_MOVE)]
        [Parser(Opcode.SMSG_MONSTER_MOVE_TRANSPORT)]
        public static void HandleMonsterMove(Packet packet)
        {
            var guid = packet.ReadPackedGuid();
            Console.WriteLine("GUID: " + guid);

            if (packet.Opcode == Opcodes.GetOpcode(Opcode.SMSG_MONSTER_MOVE_TRANSPORT))
            {
                var transguid = packet.ReadPackedGuid();
                Console.WriteLine("Transport GUID: " + transguid);

                var transseat = packet.ReadByte();
                Console.WriteLine("Transport Seat: " + transseat);
            }

            var unkByte = packet.ReadBoolean();
            Console.WriteLine("Unk Boolean: " + unkByte); // Something to do with IsVehicleExitVoluntary ?

            var pos = packet.ReadVector3();
            Console.WriteLine("Position: " + pos);

            var curTime = packet.ReadInt32();
            Console.WriteLine("Move Ticks: " + curTime);

            var type = (SplineType)packet.ReadByte();
            Console.WriteLine("Spline Type: " + type);

            switch (type)
            {
                case SplineType.FacingSpot:
                    {
                        var spot = packet.ReadVector3();
                        Console.WriteLine("Facing Spot: " + spot);
                        break;
                    }
                case SplineType.FacingTarget:
                    {
                        var tguid = packet.ReadGuid();
                        Console.WriteLine("Facing GUID: " + tguid);
                        break;
                    }
                case SplineType.FacingAngle:
                    {
                        var angle = packet.ReadSingle();
                        Console.WriteLine("Facing Angle: " + angle);
                        break;
                    }
                case SplineType.Stop:
                    {
                        return;
                    }
            }

            var flags = (SplineFlag)packet.ReadInt32();
            Console.WriteLine("Spline Flags: " + flags);

            if (flags.HasAnyFlag(SplineFlag.AnimationTier))
            {
                var unkByte3 = (MovementAnimationState)packet.ReadByte();
                Console.WriteLine("Animation State: " + unkByte3);

                var unkInt1 = packet.ReadInt32();
                Console.WriteLine("Unk Int32 1: " + unkInt1);
            }

            var time = packet.ReadInt32();
            Console.WriteLine("Move Time: " + time);

            if (flags.HasAnyFlag(SplineFlag.Trajectory))
            {
                var speedZ = packet.ReadSingle();
                Console.WriteLine("Vertical Speed: " + speedZ);

                var unkInt2 = packet.ReadInt32();
                Console.WriteLine("Unk Int32 2: " + unkInt2);
            }

            var waypoints = packet.ReadInt32();
            Console.WriteLine("Waypoints: " + waypoints);

            var newpos = packet.ReadVector3();
            Console.WriteLine("Waypoint 0: " + newpos);

            if (flags.HasAnyFlag(SplineFlag.Flying) || flags.HasAnyFlag(SplineFlag.CatmullRom))
            {
                for (var i = 0; i < waypoints - 1; i++)
                {
                    var vec = packet.ReadVector3();
                    Console.WriteLine("Waypoint " + (i + 1) + ": " + vec);
                }
            }
            else
            {
                var mid = new Vector3();
                mid.X = (pos.X + newpos.X) * 0.5f;
                mid.Y = (pos.Y + newpos.Y) * 0.5f;
                mid.Z = (pos.Z + newpos.Z) * 0.5f;

                for (var i = 0; i < waypoints - 1; i++)
                {
                    var vec = packet.ReadPackedVector3();
                    vec.X += mid.X;
                    vec.Y += mid.Y;
                    vec.Z += mid.Z;

                    Console.WriteLine("Waypoint " + (i + 1) + ": " + vec);
                }
            }
        }

        [Parser(Opcode.SMSG_NEW_WORLD)]
        [Parser(Opcode.SMSG_LOGIN_VERIFY_WORLD)]
        public static void HandleEnterWorld(Packet packet)
        {
            var mapId = packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Map ID");

            CurrentMapId = (uint)mapId;

            var position = packet.ReadVector4();
            Console.WriteLine("Position: " + position);
            CurrentPosition = position;

            UpdateHandler.Objects[CurrentMapId] = new Dictionary<Guid, WoWObject>();

            if (packet.Opcode != Opcodes.GetOpcode(Opcode.SMSG_LOGIN_VERIFY_WORLD))
                return;

            CharacterInfo chInfo;
            if (!CharacterHandler.Characters.TryGetValue(SessionHandler.LoginGuid, out chInfo))
                return;

            SessionHandler.LoggedInCharacter = chInfo;
        }

        [Parser(Opcode.SMSG_LOGIN_SETTIMESPEED)]
        public static void HandleLoginSetTimeSpeed(Packet packet)
        {
            packet.ReadPackedTime("Game Time");
            packet.ReadSingle("Game Speed");

            if (ClientVersion.AddedInVersion(ClientVersionBuild.V3_1_2_9901))
                packet.ReadInt32("Unk Int32");
        }

        [Parser(Opcode.SMSG_BINDPOINTUPDATE)]
        public static void HandleBindPointUpdate(Packet packet)
        {
            packet.ReadVector3("Position");

            packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Map ID");

            packet.ReadInt32("Zone ID");
        }

        [Parser(Opcode.MSG_MOVE_TELEPORT_ACK)]
        public static void HandleTeleportAck(Packet packet)
        {
            if (packet.Direction == Direction.ServerToClient)
            {
                var guid = packet.ReadPackedGuid();
                Console.WriteLine("GUID: " + guid);

                var counter = packet.ReadInt32();
                Console.WriteLine("Movement Counter: " + counter);

                ReadMovementInfo(packet, guid);
            }
            else
            {
                var guid = packet.ReadPackedGuid();
                Console.WriteLine("GUID: " + guid);

                var flags = (MovementFlag)packet.ReadInt32();
                Console.WriteLine("Move Flags: " + flags);

                var time = packet.ReadInt32();
                Console.WriteLine("Time: " + time);
            }
        }

        [Parser(Opcode.MSG_MOVE_START_FORWARD)]
        [Parser(Opcode.MSG_MOVE_START_BACKWARD)]
        [Parser(Opcode.MSG_MOVE_STOP)]
        [Parser(Opcode.MSG_MOVE_START_STRAFE_LEFT)]
        [Parser(Opcode.MSG_MOVE_START_STRAFE_RIGHT)]
        [Parser(Opcode.MSG_MOVE_STOP_STRAFE)]
        [Parser(Opcode.MSG_MOVE_START_ASCEND)]
        [Parser(Opcode.MSG_MOVE_START_DESCEND)]
        [Parser(Opcode.MSG_MOVE_STOP_ASCEND)]
        [Parser(Opcode.MSG_MOVE_JUMP)]
        [Parser(Opcode.MSG_MOVE_START_TURN_LEFT)]
        [Parser(Opcode.MSG_MOVE_START_TURN_RIGHT)]
        [Parser(Opcode.MSG_MOVE_STOP_TURN)]
        [Parser(Opcode.MSG_MOVE_START_PITCH_UP)]
        [Parser(Opcode.MSG_MOVE_START_PITCH_DOWN)]
        [Parser(Opcode.MSG_MOVE_STOP_PITCH)]
        [Parser(Opcode.MSG_MOVE_SET_RUN_MODE)]
        [Parser(Opcode.MSG_MOVE_SET_WALK_MODE)]
        [Parser(Opcode.MSG_MOVE_TELEPORT)]
        [Parser(Opcode.MSG_MOVE_SET_FACING)]
        [Parser(Opcode.MSG_MOVE_SET_PITCH)]
        [Parser(Opcode.MSG_MOVE_TOGGLE_COLLISION_CHEAT)]
        [Parser(Opcode.MSG_MOVE_GRAVITY_CHNG)]
        [Parser(Opcode.MSG_MOVE_ROOT)]
        [Parser(Opcode.MSG_MOVE_UNROOT)]
        [Parser(Opcode.MSG_MOVE_START_SWIM)]
        [Parser(Opcode.MSG_MOVE_STOP_SWIM)]
        [Parser(Opcode.MSG_MOVE_START_SWIM_CHEAT)]
        [Parser(Opcode.MSG_MOVE_STOP_SWIM_CHEAT)]
        [Parser(Opcode.MSG_MOVE_HEARTBEAT)]
        [Parser(Opcode.MSG_MOVE_FALL_LAND)]
        [Parser(Opcode.MSG_MOVE_UPDATE_CAN_FLY)]
        [Parser(Opcode.MSG_MOVE_UPDATE_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY)]
        [Parser(Opcode.MSG_MOVE_KNOCK_BACK)]
        [Parser(Opcode.MSG_MOVE_HOVER)]
        [Parser(Opcode.MSG_MOVE_FEATHER_FALL)]
        [Parser(Opcode.MSG_MOVE_WATER_WALK)]
        [Parser(Opcode.CMSG_MOVE_FALL_RESET)]
        [Parser(Opcode.CMSG_MOVE_SET_FLY)]
        [Parser(Opcode.CMSG_MOVE_CHNG_TRANSPORT)]
        [Parser(Opcode.CMSG_MOVE_NOT_ACTIVE_MOVER)]
        [Parser(Opcode.CMSG_DISMISS_CONTROLLED_VEHICLE)]
        public static void HandleMovementMessages(Packet packet)
        {
            Guid guid;
            if (ClientVersion.AddedInVersion(ClientVersionBuild.V3_2_0_10192) ||
                packet.Direction == Direction.ServerToClient)
                guid = packet.ReadPackedGuid("GUID");
            else
                guid = new Guid();

            ReadMovementInfo(packet, guid);
            if (packet.Opcode == Opcodes.GetOpcode(Opcode.MSG_MOVE_KNOCK_BACK))
            {
                packet.ReadSingle("Sin Angle");
                packet.ReadSingle("Cos Angle");
                packet.ReadSingle("Speed");
                packet.ReadSingle("Velocity");
            }
        }

        [Parser(Opcode.MSG_MOVE_SET_WALK_SPEED)]
        [Parser(Opcode.MSG_MOVE_SET_RUN_SPEED)]
        [Parser(Opcode.MSG_MOVE_SET_RUN_BACK_SPEED)]
        [Parser(Opcode.MSG_MOVE_SET_SWIM_SPEED)]
        [Parser(Opcode.MSG_MOVE_SET_SWIM_BACK_SPEED)]
        [Parser(Opcode.MSG_MOVE_SET_TURN_RATE)]
        [Parser(Opcode.MSG_MOVE_SET_FLIGHT_SPEED)]
        [Parser(Opcode.MSG_MOVE_SET_FLIGHT_BACK_SPEED)]
        [Parser(Opcode.MSG_MOVE_SET_PITCH_RATE)]
        public static void HandleMovementSetSpeed(Packet packet)
        {
            var guid = packet.ReadPackedGuid("GUID");

            ReadMovementInfo(packet, guid);

            packet.ReadSingle("Speed");
        }

        [Parser(Opcode.CMSG_FORCE_RUN_SPEED_CHANGE_ACK)]
        [Parser(Opcode.CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK)]
        [Parser(Opcode.CMSG_FORCE_SWIM_SPEED_CHANGE_ACK)]
        [Parser(Opcode.CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK)]
        [Parser(Opcode.CMSG_FORCE_WALK_SPEED_CHANGE_ACK)]
        [Parser(Opcode.CMSG_FORCE_TURN_RATE_CHANGE_ACK)]
        [Parser(Opcode.CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK)]
        [Parser(Opcode.CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK)]
        public static void HandleSpeedChangeMessage(Packet packet)
        {
            var guid = packet.ReadPackedGuid();
            Console.WriteLine("GUID: " + guid);

            var counter = packet.ReadInt32();
            Console.WriteLine("Movement Counter: " + counter);

            ReadMovementInfo(packet, guid);

            var newSpeed = packet.ReadSingle();
            Console.WriteLine("New Speed: " + newSpeed);
        }

        [Parser(Opcode.MSG_MOVE_SET_COLLISION_HGT)]
        [Parser(Opcode.SMSG_MOVE_SET_COLLISION_HGT)]
        [Parser(Opcode.CMSG_MOVE_SET_COLLISION_HGT_ACK)]
        public static void HandleCollisionMovements(Packet packet)
        {
            var guid = packet.ReadPackedGuid();
            Console.WriteLine("GUID: " + guid);

            if (packet.Opcode != Opcodes.GetOpcode(Opcode.MSG_MOVE_SET_COLLISION_HGT))
            {
                var counter = packet.ReadInt32();
                Console.WriteLine("Movement Counter: " + counter);
            }

            if (packet.Opcode != Opcodes.GetOpcode(Opcode.SMSG_MOVE_SET_COLLISION_HGT))
                ReadMovementInfo(packet, guid);

            var unk = packet.ReadSingle();
            Console.WriteLine("Collision Height: " + unk);
        }

        [Parser(Opcode.CMSG_SET_ACTIVE_MOVER)]
        [Parser(Opcode.SMSG_MOUNTSPECIAL_ANIM)]
        public static void HandleSetActiveMover(Packet packet)
        {
            var guid = packet.ReadGuid();
            Console.WriteLine("GUID: " + guid);
        }

        [Parser(Opcode.SMSG_SUMMON_REQUEST)]
        public static void HandleSummonRequest(Packet packet)
        {
            packet.ReadGuid("Summoner GUID");
            packet.ReadInt32("Unk int 1");
            packet.ReadInt32("Unk int 2");
        }

        [Parser(Opcode.CMSG_SUMMON_RESPONSE)]
        public static void HandleSummonResponse(Packet packet)
        {
            packet.ReadGuid("Summoner GUID");
            packet.ReadBoolean("Accept");
        }

        [Parser(Opcode.SMSG_FORCE_MOVE_ROOT)]
        [Parser(Opcode.SMSG_FORCE_MOVE_UNROOT)]
        [Parser(Opcode.SMSG_MOVE_WATER_WALK)]
        [Parser(Opcode.SMSG_MOVE_LAND_WALK)]
        public static void HandleSetMovementMessages(Packet packet)
        {
            var guid = packet.ReadPackedGuid();
            Console.WriteLine("GUID: " + guid);

            var counter = packet.ReadInt32();
            Console.WriteLine("Movement Counter: " + counter);
        }

        [Parser(Opcode.CMSG_MOVE_KNOCK_BACK_ACK)]
        [Parser(Opcode.CMSG_MOVE_WATER_WALK_ACK)]
        [Parser(Opcode.CMSG_MOVE_HOVER_ACK)]
        public static void HandleSpecialMoveAckMessages(Packet packet)
        {
            var guid = packet.ReadPackedGuid();
            Console.WriteLine("GUID: " + guid);

            var unk1 = packet.ReadInt32();
            Console.WriteLine("Unk Int32 1: " + unk1);

            ReadMovementInfo(packet, guid);

            if (packet.Opcode == Opcodes.GetOpcode(Opcode.CMSG_MOVE_KNOCK_BACK_ACK))
                return;

            var unk2 = packet.ReadInt32();
            Console.WriteLine("Unk Int32 2: " + unk2);
        }

        [Parser(Opcode.SMSG_SET_PHASE_SHIFT)]
        public static void HandlePhaseShift(Packet packet)
        {
            var phaseMask = packet.ReadInt32();
            Console.WriteLine("Phase Mask: 0x" + phaseMask.ToString("X8"));
            CurrentPhaseMask = phaseMask;
        }

        [Parser(Opcode.SMSG_TRANSFER_PENDING)]
        public static void HandleTransferPending(Packet packet)
        {
            packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Map ID");

            if (!packet.CanRead())
                return;

            var tEntry = packet.ReadInt32("Transport Entry");
            Console.WriteLine("Transport Entry: " + tEntry);

            packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Transport Map ID");
        }

        [Parser(Opcode.SMSG_TRANSFER_ABORTED)]
        public static void HandleTransferAborted(Packet packet)
        {
            packet.ReadEntryWithName<Int32>(StoreNameType.Map, "Map ID");

            var code = (TransferAbortReason)packet.ReadByte();
            Console.WriteLine("Reason: " + code);

            switch (code)
            {
                case TransferAbortReason.DifficultyUnavailable:
                    {
                        var arg = (MapDifficulty)packet.ReadByte();
                        Console.WriteLine("Difficulty: " + arg);
                        break;
                    }
                case TransferAbortReason.InsufficientExpansion:
                    {
                        var arg = (ClientType)packet.ReadByte();
                        Console.WriteLine("Expansion: " + arg);
                        break;
                    }
                case TransferAbortReason.UniqueMessage:
                    {
                        var arg = packet.ReadByte();
                        Console.WriteLine("Message ID: " + arg);
                        break;
                    }
            }
        }

        [Parser(Opcode.SMSG_FLIGHT_SPLINE_SYNC)]
        public static void HandleFlightSplineSync(Packet packet)
        {
            var val = packet.ReadSingle();
            Console.WriteLine("Unk Single: " + val);

            var guid = packet.ReadPackedGuid();
            Console.WriteLine("GUID: " + guid);
        }

        [Parser(Opcode.SMSG_CLIENT_CONTROL_UPDATE)]
        public static void HandleClientControlUpdate(Packet packet)
        {
            packet.ReadPackedGuid("GUID");
            packet.ReadByte("AllowMove");
        }

        [Parser(Opcode.SMSG_MOVE_KNOCK_BACK)]
        public static void HandleMoveKnockBack(Packet packet)
        {
            packet.ReadPackedGuid("GUID");
            packet.ReadUInt32("Counter");
            packet.ReadSingle("X direction");
            packet.ReadSingle("Y direction");
            packet.ReadSingle("Horizontal Speed");
            packet.ReadSingle("Vertical Speed");
        }
    }
}
