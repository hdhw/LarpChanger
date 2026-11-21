#pragma once

#include "sdk/client/CModelState.h"
#include "sdk/macros.h"

class CSkeletonInstance {
public:
  SDK_CLASS(CSkeletonInstance);

  SCHEMA_FIELD(CModelState, m_modelState);
};

class CBodyComponentSkeletonInstance {
public:
  SDK_CLASS(CBodyComponentSkeletonInstance);

  SCHEMA_FIELD(CSkeletonInstance, m_skeletonInstance);
};
