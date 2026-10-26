// Checks the unmodified MIT SkyCraft wire layout on both Windows architectures.
#include "../upstream/SkyCraft/protocol/skycraft_protocol.h"
#include <cstddef>
#include <cstdio>
int main() {
    using namespace skycraft::proto;
    std::printf("{\"version\":%u,\"header\":%zu,\"skyState\":%zu,\"mcState\":%zu,"
        "\"actor\":%zu,\"event\":%zu,\"vertex\":%zu,\"mappingBytes\":%llu,"
        "\"skyPositionOffset\":%zu,\"mcHeartbeatOffset\":%zu,"
        "\"collisionOffset\":%llu,\"collisionBytes\":%llu,\"ringHead\":%llu,\"ringTail\":%llu,\"ringData\":%llu,"
        "\"skyOffset\":%llu,\"mcOffset\":%llu,\"actorOffset\":%llu}\n",
        kVersion,sizeof(Header),sizeof(SkyState),sizeof(McState),sizeof(ActorRecord),
        sizeof(McEvent),sizeof(RenVertex),static_cast<unsigned long long>(kMappingBytes),
        offsetof(SkyState,posX),offsetof(Header,mcHeartbeatMs),
        (unsigned long long)kOffCollisionRing,(unsigned long long)kCollisionRingBytes,
        (unsigned long long)kColRingHeadOff,(unsigned long long)kColRingTailOff,(unsigned long long)kColRingDataOff,
        (unsigned long long)kOffSkyState,(unsigned long long)kOffMcState,(unsigned long long)kOffActorTable);
}
