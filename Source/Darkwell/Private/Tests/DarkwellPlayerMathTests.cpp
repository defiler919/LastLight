// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/DarkwellPlayerMath.h"
#include "Player/DarkwellWeaponWheelRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellWeaponWheelInputTest,
	"Darkwell.Player.Input.WeaponWheels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellWeaponWheelInputTest::RunTest(const FString& Parameters)
{
	using Darkwell::WeaponWheelRules::ResolveActiveWheel;

	TestEqual(
		TEXT("Holding Q opens the left wheel"),
		ResolveActiveWheel(true, false, false, false, EDarkwellWeaponWheelSide::None),
		EDarkwellWeaponWheelSide::Left);
	TestEqual(
		TEXT("Releasing Q always closes the wheel"),
		ResolveActiveWheel(false, false, true, false, EDarkwellWeaponWheelSide::Left),
		EDarkwellWeaponWheelSide::None);
	TestEqual(
		TEXT("Holding E opens the right wheel"),
		ResolveActiveWheel(false, true, false, false, EDarkwellWeaponWheelSide::None),
		EDarkwellWeaponWheelSide::Right);
	TestEqual(
		TEXT("Pressing E while Q remains held switches to the right wheel"),
		ResolveActiveWheel(true, true, true, false, EDarkwellWeaponWheelSide::Left),
		EDarkwellWeaponWheelSide::Right);
	TestEqual(
		TEXT("Releasing E while Q remains held returns to the left wheel"),
		ResolveActiveWheel(true, false, true, true, EDarkwellWeaponWheelSide::Right),
		EDarkwellWeaponWheelSide::Left);
	TestTrue(
		TEXT("Releasing an open E wheel commits the right-hand selection"),
		Darkwell::WeaponWheelRules::ShouldCycleRightHandItem(false, true, EDarkwellWeaponWheelSide::Right));
	TestFalse(
		TEXT("A stray E release cannot change equipment"),
		Darkwell::WeaponWheelRules::ShouldCycleRightHandItem(false, true, EDarkwellWeaponWheelSide::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellPlanarAimTest,
	"Darkwell.Player.Aim.PlanarDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellPlanarAimTest::RunTest(const FString& Parameters)
{
	FVector Direction;
	TestTrue(
		TEXT("A target's height does not affect its planar direction"),
		Darkwell::PlayerMath::TryGetPlanarDirection(
			FVector(100.0, 50.0, 10.0),
			FVector(100.0, 250.0, 900.0),
			Direction));
	TestTrue(TEXT("The planar direction is normalized on XY"), Direction.Equals(FVector::YAxisVector, UE_KINDA_SMALL_NUMBER));

	TestFalse(
		TEXT("A target directly above the character has no usable planar direction"),
		Darkwell::PlayerMath::TryGetPlanarDirection(
			FVector(10.0, 20.0, 30.0),
			FVector(10.0, 20.0, 500.0),
			Direction));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellLimitedTurnTest,
	"Darkwell.Player.Aim.LimitedTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellLimitedTurnTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Turning cannot exceed the configured angular speed"),
		Darkwell::PlayerMath::TurnYawToward(0.0f, 90.0f, 90.0f, 0.5f),
		45.0f);
	TestEqual(
		TEXT("Turning uses the shortest path across the yaw seam"),
		Darkwell::PlayerMath::TurnYawToward(170.0f, -170.0f, 20.0f, 0.5f),
		180.0f);
	TestEqual(
		TEXT("Negative delta time cannot rotate the player"),
		Darkwell::PlayerMath::TurnYawToward(25.0f, 90.0f, 120.0f, -1.0f),
		25.0f);
	TestTrue(
		TEXT("A held sprint input with movement enters sprint"),
		Darkwell::PlayerMath::ShouldSprint(true, true, FVector::XAxisVector));
	TestFalse(
		TEXT("Holding sprint while stationary stays in walking state"),
		Darkwell::PlayerMath::ShouldSprint(true, true, FVector::ZeroVector));
	TestFalse(
		TEXT("Menus and inventory block sprint state"),
		Darkwell::PlayerMath::ShouldSprint(true, false, FVector::XAxisVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellCursorPlaneIntersectionTest,
	"Darkwell.Player.Aim.CursorPlaneIntersection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellCursorPlaneIntersectionTest::RunTest(const FString& Parameters)
{
	FVector Intersection;
	const FVector DownwardDiagonal = FVector(1.0, 0.0, -1.0).GetSafeNormal();
	TestTrue(
		TEXT("A downward cursor ray intersects the character's horizontal plane"),
		Darkwell::PlayerMath::TryIntersectHorizontalPlane(
			FVector(0.0, 0.0, 1000.0),
			DownwardDiagonal,
			0.0f,
			Intersection));
	TestTrue(TEXT("The intersection is in front of the ray"), Intersection.Equals(FVector(1000.0, 0.0, 0.0), 0.01));

	TestFalse(
		TEXT("A horizontal ray does not intersect a distinct horizontal plane"),
		Darkwell::PlayerMath::TryIntersectHorizontalPlane(
			FVector(0.0, 0.0, 1000.0),
			FVector::XAxisVector,
			0.0f,
			Intersection));
	TestFalse(
		TEXT("An upward ray rejects an intersection behind its origin"),
		Darkwell::PlayerMath::TryIntersectHorizontalPlane(
			FVector(0.0, 0.0, 1000.0),
			FVector::ZAxisVector,
			0.0f,
			Intersection));
	return true;
}

#endif
