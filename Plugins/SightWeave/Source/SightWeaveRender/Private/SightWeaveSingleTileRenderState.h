#pragma once

#include "RenderGraphResources.h"
#include "SightWeaveRenderPacket.h"
#include "SightWeaveRenderWorldSubsystem.h"

class FRDGBuilder;
struct IPooledRenderTarget;

#if WITH_DEV_AUTOMATION_TESTS
struct FSightWeaveRenderPassSetupTimings
{
	double ClearMicroseconds = 0.0;
	double RasterVisionMicroseconds = 0.0;
	double RasterIlluminationMicroseconds = 0.0;
	double RasterBypassMicroseconds = 0.0;
	double RasterSuppressionMicroseconds = 0.0;
	double CombineMicroseconds = 0.0;
};
#endif

class FSightWeaveSingleTileRenderState final
{
public:
	explicit FSightWeaveSingleTileRenderState(FSightWeaveRenderWorldIdentity InWorldIdentity);

	void SubmitPacket_RenderThread(
		const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>& Packet);
	FRDGTextureRef ProcessPending_RenderThread(FRDGBuilder& GraphBuilder);
	void Release_RenderThread(FSightWeaveRenderWorldIdentity ExpectedWorldIdentity);

	ESightWeaveRenderAvailability GetAvailability_RenderThread() const { return Availability; }
	uint64 GetDesiredRevision_RenderThread() const { return DesiredRevision; }
	uint64 GetAppliedRevision_RenderThread() const { return AppliedRevision; }
	uint64 GetRasterDispatchCount_RenderThread() const { return RasterDispatchCount; }
	uint64 GetResourceGeneration_RenderThread() const { return ResourceGeneration; }
	uint64 GetDuplicatePacketCount_RenderThread() const { return DuplicatePacketCount; }
	uint64 GetStalePacketCount_RenderThread() const { return StalePacketCount; }
	uint64 GetRejectedPacketCount_RenderThread() const { return RejectedPacketCount; }
#if WITH_DEV_AUTOMATION_TESTS
	const FSightWeaveRenderPassSetupTimings& GetLastPassSetupTimings_RenderThread() const
	{
		return LastPassSetupTimings;
	}
#endif

private:
	bool CheckCapabilities_RenderThread();
	bool EnsurePersistentTexture_RenderThread();
	FRDGTextureRef AddBlackClearPass_RenderThread(FRDGBuilder& GraphBuilder);
	FRDGTextureRef AddRasterPasses_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSightWeaveRenderPacket& Packet);

	FSightWeaveRenderWorldIdentity WorldIdentity;
	TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> PendingPacket;
	TRefCountPtr<IPooledRenderTarget> EffectiveLiveTexture;
	uint64 DesiredRevision = 0;
	uint64 DesiredHash = 0;
	uint64 AppliedRevision = 0;
	uint64 RasterDispatchCount = 0;
	uint64 DuplicatePacketCount = 0;
	uint64 StalePacketCount = 0;
	uint64 RejectedPacketCount = 0;
	uint64 ResourceGeneration = 0;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	bool bPendingForceBlack = false;
	bool bReleased = false;
#if WITH_DEV_AUTOMATION_TESTS
	FSightWeaveRenderPassSetupTimings LastPassSetupTimings;
#endif
};
