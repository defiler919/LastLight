#pragma once

#include "RenderGraphResources.h"
#include "SightWeaveRenderPacket.h"
#include "SightWeaveRenderWorldSubsystem.h"

class FRDGBuilder;
struct IPooledRenderTarget;

class FSightWeaveSingleTileRenderState final
{
public:
	explicit FSightWeaveSingleTileRenderState(FSightWeaveRenderWorldIdentity InWorldIdentity);

	void SubmitPacket_RenderThread(
		const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>& Packet);
	void ProcessPending_RenderThread(FRDGBuilder& GraphBuilder);
	void Release_RenderThread(FSightWeaveRenderWorldIdentity ExpectedWorldIdentity);

	ESightWeaveRenderAvailability GetAvailability_RenderThread() const { return Availability; }
	uint64 GetDesiredRevision_RenderThread() const { return DesiredRevision; }
	uint64 GetAppliedRevision_RenderThread() const { return AppliedRevision; }
	uint64 GetRasterDispatchCount_RenderThread() const { return RasterDispatchCount; }

private:
	bool CheckCapabilities_RenderThread();
	bool EnsurePersistentTexture_RenderThread();
	void AddBlackClearPass_RenderThread(FRDGBuilder& GraphBuilder);
	void AddRasterPasses_RenderThread(
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
};
