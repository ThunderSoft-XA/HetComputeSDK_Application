/** @file devicetypes.hh */
#pragma once

#include <initializer_list>
#include <string>
#include <hetcompute/internal/util/macros.hh>

/**
 *  hetcompute_device_type_t indicates a specific device on which any operation is performed.
 */
typedef enum {
    // bitmask, one device a bit
    HETCOMPUTE_DEVICE_NONE            = 0x0,
    HETCOMPUTE_DEVICE_TYPE_CPU_BIG    = 0x01,
    HETCOMPUTE_DEVICE_TYPE_CPU_LITTLE = 0x02,
    HETCOMPUTE_DEVICE_TYPE_GPU        = 0x04,
    HETCOMPUTE_DEVICE_TYPE_DSP        = 0x08,
    HETCOMPUTE_DEVICE_TYPE_ALL        = 0x0F
} hetcompute_device_type_t;

/**
 * hetcompute_device_set_t aggregates all the devices on which any operation is performed
 *
 * - For example:
 *   <code> hetcompute_device_set_t devices = HETCOMPUTE_DEVICE_TYPE_CPU_BIG |
 *   HETCOMPUTE_DEVICE_TYPE_CPU_LITTLE</code>
 */
typedef unsigned int hetcompute_device_set_t;

namespace hetcompute
{
    class device_set;

    namespace internal
    {
        hetcompute_device_set_t get_raw_device_set_t(device_set const& d);
    }; // namespace internal

    /** @addtogroup devicetypes_doc
        @{ */

    /**
     *  @brief The system devices capable of executing HetCompute tasks.
     *
     *  The system devices capable of executing HetCompute tasks.
     */
    enum device_type
    {
        cpu_big    = HETCOMPUTE_DEVICE_TYPE_CPU_BIG,
        cpu_little = HETCOMPUTE_DEVICE_TYPE_CPU_LITTLE,
        cpu        = HETCOMPUTE_DEVICE_TYPE_CPU_BIG | HETCOMPUTE_DEVICE_TYPE_CPU_LITTLE,
        gpu        = HETCOMPUTE_DEVICE_TYPE_GPU,
        dsp        = HETCOMPUTE_DEVICE_TYPE_DSP,
    };

    /**
     *  @brief Converts <code>device_type</code> to string.
     *
     *  Converts <code>device_type</code> to string.
     *
     *  @param d device_type (e.g., cpu, cpu_big, cpu_little, gpu, or dsp).
     *  @return std::string (e.g., "cpu", "cpu_big", "cpu_little", "gpu", or "dsp").
     */

    std::string to_string(device_type d);

    /**
     *  @brief Captures a set of device types.
     *
     *  Captures a set of device types.
     *
     *  Supports addition and removal of <code>device_type</code>s from the set.
     *  Supports set union and set subtraction with another device_set object.
     */
    class device_set
    {
        hetcompute_device_set_t _device_set_mask;

    public:
        /**
         *  @brief Default constructor produces empty set.
         *
         *  Default constructor produces empty set.
         */
        device_set();

        /**
         *  @brief Constructor with initialization.
         *
         *  Constructor with initialization.
         *
         *  @param device_list nn initializer list of <code>device_type</code> elements.
         *
         *  Example:\n
         *  <code>
         *    hetcompute::device_set ds{hetcompute::cpu, hetcompute::dsp};
         *  </code>
         */
        device_set(std::initializer_list<device_type> device_list);

        /**
        *  @brief Checks if device set has any devices or its empty
        *
        *  Checks if device set has any devices or its empty.
        *
        *  @return
        *  <code>true</code> if device_set has no devices,\n
        *  <code>false</code> otherwise.
        */
        bool empty() const;

        /**
         *  @brief Query if cpu is part of the device_set.
         *
         *  Query if cpu is part of the device_set.
         *
         *  @return
         *  <code>true</code> if cpu present,\n
         *  <code>false</code> otherwise.
         */
        bool on_cpu() const;

        /**
         *  @brief Query if cpu big core is part of the device_set.
         *
         *  Query if cpu big core is part of the device_set.
         *
         *  @return
         *  <code>true</code> if cpu big core present,\n
         *  <code>false</code> otherwise.
         */
        bool on_cpu_big() const;

        /**
         *  @brief Query if cpu LITTLE core is part of the device_set.
         *
         *  Query if cpu LITTLE core is part of the device_set.
         *
         *  @return
         *  <code>true</code> if cpu LITTLE core present,\n
         *  <code>false</code> otherwise.
         */
        bool on_cpu_little() const;

        /**
         *  @brief Query if gpu is part of the device_set.
         *
         *  Query if gpu is part of the device_set.
         *
         *  @return
         *  <code>true</code> if gpu present,\n
         *  <code>false</code> otherwise.
         */
        bool on_gpu() const;

        /**
         *  @brief Query if dsp is part of the device_set.
         *
         *  Query if dsp is part of the device_set.
         *
         *  @return
         *  <code>true</code> if dsp present,\n
         *  <code>false</code> otherwise.
         */
        bool on_dsp() const;

        /**
         *  @brief Add a <code>device_type</code> to the <code>device_set</code>.
         *
         *  Add a <code>device_type</code> to the <code>device_set</code>.
         *
         *  @param d <code>device_type</code> to add (no effect if already present).
         *
         *  @return Reference to the updated <code>device_set</code>.
         */
        device_set& add(device_type d);

        /**
         *  @brief Set union with another <code>device_set</code>.
         *
         *  Set union with another <code>device_set</code>.
         *
         *  @param other Another <code>device_set</code>.
         *
         *  @return Reference to the updated <code>device_set</code>.
         *
         *  Example:\n
         *  <code>
         *    hetcompute::device_set a{hetcompute::cpu};\n
         *    hetcompute::device_set b{hetcompute::gpu};\n
         *    a.add(b);\n
         *    assert(true == a.on_cpu());\n
         *    assert(true == a.on_gpu());\n
         *    assert(false == a.on_dsp());\n
         *  </code>
         */
        device_set& add(device_set const& other);

        /**
         *  @brief Remove a device_type from the device_set.
         *
         *  Remove a device_type from the device_set.
         *
         *  @param d device_type to remove (no effect if not present).
         *
         *  @return Reference to the updated device_set.
         */
        device_set& remove(device_type d);

        /**
         *  @brief Set substraction with another device_set.
         *
         *  Set substraction with another device_set.
         *
         *  @param other Another device_set.
         *
         *  @return Reference to the updated device_set.
         *
         *  Example:\n
         *  <code>
         *    hetcompute::device_set a{hetcompute::cpu, hetcompute::gpu};\n
         *    hetcompute::device_set b{hetcompute::gpu, hetcompute::dsp};\n
         *    a.remove(b);\n
         *    assert(true == a.on_cpu());\n
         *    assert(false == a.on_gpu());\n
         *    assert(false == a.on_dsp());\n
         *  </code>
         */
        device_set& remove(device_set const& other);

        /**
         *  @brief Negate the device_set.
         *
         *  Negate the device_set.
         *
         *  @return Reference to the updated device_set.
         *
         *  Example:\n
         *  <code>
         *    hetcompute::device_set a{hetcompute::cpu, hetcompute::gpu};\n
         *    a.negate();\n
         *    assert(false == a.on_cpu());\n
         *    assert(false == a.on_gpu());\n
         *    assert(true == a.on_dsp());\n
         *  </code>
         */
        device_set& negate();

        /**
         *  @brief Convert the device_set to a string representation.
         *
         *  Convert the device_set to a string representation.
         *
         *  @return std::string representation of the devices present.
         *
         *  Example:\n
         *  <code>
         *    hetcompute::device_set a{hetcompute::cpu, hetcompute::gpu};\n
         *    assert(a.to_string() == "cpu gpu ");\n
         *  </code>
         */
        std::string to_string() const;

        /**
         * Copy constructor.
         */
        HETCOMPUTE_DEFAULT_METHOD(device_set(device_set const&));

        /**
         * Copy assignment.
         */
        HETCOMPUTE_DEFAULT_METHOD(device_set& operator=(device_set const&));

        /**
         * Move constructor.
         */
        HETCOMPUTE_DEFAULT_METHOD(device_set(device_set&&));

        /**
         * Move assignment.
         */
        HETCOMPUTE_DEFAULT_METHOD(device_set& operator=(device_set&&));

        friend hetcompute_device_set_t internal::get_raw_device_set_t(device_set const& d);
    };

    /** @} */ /* end_addtogroup devicetypes_doc */

}; // namespace hetcompute
