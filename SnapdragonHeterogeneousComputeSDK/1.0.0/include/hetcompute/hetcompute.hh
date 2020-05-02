/** @file hetcompute.hh */
#pragma once

// verify hetcompute supports the compiler
#include <hetcompute/internal/compat/compiler_check.h>
#include <hetcompute/internal/compat/compat.h>

#include <hetcompute/affinity.hh>
#include <hetcompute/buffer.hh>
#include <hetcompute/index.hh>
#include <hetcompute/range.hh>
#include <hetcompute/runtime.hh>

#include <hetcompute/group.hh>
#include <hetcompute/groupptr.hh>
#include <hetcompute/patterns.hh>
#include <hetcompute/pointkernel.hh>

#include <hetcompute/schedulerstorage.hh>
#include <hetcompute/scopedstorage.hh>
#include <hetcompute/taskstorage.hh>
#include <hetcompute/threadstorage.hh>

#include <hetcompute/taskfactory.hh>
#include <hetcompute/texture.hh>

#include <hetcompute/internal/task/task_impl.hh>
#include <hetcompute/hetcompute_error.hh>
