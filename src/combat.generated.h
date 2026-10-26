// Generated combat rules. Acceptance testing remains required.
#pragma once
namespace classic::combat {
struct HealthModifier { static constexpr unsigned value=2u; };
struct DamageBaseScale { static constexpr unsigned value=5u; };
struct DamageLevelScale { static constexpr float value=0.25f; };
struct DiagnosticEnemyDistance { static constexpr unsigned value=175u; };
struct DiagnosticMinimumHeight { static constexpr float value=1.4f; };
struct EquipmentSettleMs { static constexpr unsigned value=600u; };
struct HitCriticalMask { static constexpr unsigned value=8u; };
struct HitSneakMask { static constexpr unsigned value=2048u; };
struct HitPowerMask { static constexpr unsigned value=65536u; };
struct ArcherySkill { static constexpr unsigned value=8u; };
struct OneHandedSkill { static constexpr unsigned value=6u; };
struct MaximumPoseGapBlocks { static constexpr float value=16.0f; };
struct CombatPoseToleranceBlocks { static constexpr float value=1.0f; };
struct ForwardTestKey { static constexpr unsigned value=26u; };
struct ForwardTestHoldMs { static constexpr unsigned value=2000u; };
struct RightTestKey { static constexpr unsigned value=7u; };
struct LeftTestKey { static constexpr unsigned value=4u; };
struct DepthBypassDefault { static constexpr unsigned value=0u; };
struct DepthBypassZValue { static constexpr unsigned value=0u; };
struct DeathDamageExtra { static constexpr float value=1.0f; };
struct KillPrimaryFlag { static constexpr unsigned value=1u; };
struct KillSecondaryFlag { static constexpr unsigned value=0u; };
struct AimTestRangeBlocks { static constexpr float value=8.0f; };
struct AimTestHeightFraction { static constexpr float value=0.6f; };
struct NativeTeleportThresholdBlocks { static constexpr float value=2.0f; };
struct MaximumCameraDistanceBlocks { static constexpr float value=8.0f; };
}
