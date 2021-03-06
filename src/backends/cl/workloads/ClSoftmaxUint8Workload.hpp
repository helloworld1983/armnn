//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <backendsCommon/Workload.hpp>

#include <arm_compute/runtime/CL/functions/CLSoftmaxLayer.h>
#include <arm_compute/runtime/MemoryManagerOnDemand.h>

#include <memory>

namespace armnn
{
// Softmax
class ClSoftmaxUint8Workload : public Uint8Workload<SoftmaxQueueDescriptor>
{
public:
    ClSoftmaxUint8Workload(const SoftmaxQueueDescriptor& descriptor, const WorkloadInfo& info,
                           std::shared_ptr<arm_compute::MemoryManagerOnDemand>& memoryManager);

    void Execute() const override;
private:

    mutable arm_compute::CLSoftmaxLayer m_SoftmaxLayer;
};

} //namespace armnn

