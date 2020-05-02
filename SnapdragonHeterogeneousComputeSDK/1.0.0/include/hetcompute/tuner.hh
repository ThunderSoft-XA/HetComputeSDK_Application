#pragma once

#include <hetcompute/runtime.hh>

namespace hetcompute
{
    namespace pattern
    {
        // Distribution of workload across iterations
        enum class shape
        {
            uniform,
            exponential,
            gaussian,
            triangle,
            inv_triangle,
            parabola,
            randif,
            hill,
            valley
        };

        /** @addtogroup pattern_tuner_doc
            @{ */

        class tuner
        {
        public:
            using load_type = size_t;

            /**
             * Tuner constructor
             * Parameters to fine-tune various execution settings in HETCOMPUTE patterns.
             * Note that tuner settings are hints that the HETCOMPUTE runtime takes
             * into account while scheduling a pattern. Constraining factors may
             * cause HETCOMPUTE to ignore the hints.
             */
            tuner()
                : _max_doc(hetcompute::internal::num_execution_contexts()),
                  _min_chunk_size(1),
                  _static(false),
                  _shape(shape::uniform),
                  _serialize(false),
                  _user_setbit(false),
                  _cpu_load(0),
                  _dsp_load(0),
                  _gpu_load(0),
                  _profile(false)
            {
                HETCOMPUTE_INTERNAL_ASSERT(_max_doc > 0, "Degree of Concurrency must be > 0!");
                HETCOMPUTE_INTERNAL_ASSERT(_min_chunk_size > 0, "Chunk size must be > 0!");
            }

            /**
             * Defines the maximum number of tasks in parallel (degree of concurrency)
             * for load balancing. A higher number indicates over-subscription which might
             * be beneficial in certain usage scenarios. <tt>doc</tt> must be larger
             * than zero. Otherwise, it will cause a fatal error.
             *
             *
             * @param doc Degree of concurrency, set to the number of available cores
             *            by default.
             * @return tuner& reference to the tuner object
             */
            tuner& set_max_doc(size_t doc)
            {
                HETCOMPUTE_API_ASSERT(doc > 0, "Degree of concurrency must be > 0!");

                _max_doc = doc;
                return *this;
            }

            /**
             * Query the maximum number of tasks launched in parallel.
             *
             * @return size_t degree of concurrency.
             */

            size_t get_doc() const { return _max_doc; }

            /**
             * Defines granularity for work stealing. In data parallel patterns,
             * Qualcomm HETCOMPUTE launches multiple tasks (defined by doc) in parallel.
             * Each task steals some iterations from other tasks when its assigned
             * iterations are completed. The chunk size parameter controls the
             * minimum number of iterations a task needs to finish before it is
             * stolen from a stealer task. It is recommended to increase chunk
             * size when the computation in each iteration is less.
             *
             * @param sz Minimum chunk size.
             * @return tuner& reference to the tuner object.
             */

            tuner& set_chunk_size(size_t sz)
            {
                HETCOMPUTE_API_ASSERT(sz > 0, "Chunk size must be > 0!");

                _min_chunk_size = sz;
                // if user sets chunk size, don't trigger default chunk assignment
                // in preduce
                _user_setbit = true;
                return *this;
            }

            /**
             * Query the granularity of work stealing.
             *
             * @return size_t minimum chunk size.
             */

            size_t get_chunk_size() const { return _min_chunk_size; }

            /// @cond
            // No need to expose
            bool is_chunk_set() const { return _user_setbit; }
            /// @endcond

            /**
             * Set the parallelization algorithm to static chunking.
             *
             * The static chunking algorithm simply divides the iteration range
             * equally and allocates the chunks to parallel launched tasks. This
             * algorithm has lower synchronization overhead compared with the
             * default dynamic work stealing algorithm, and features good locality.
             * However, it does not provide load balancing, and in most cases is
             * outperformed by the dynamic work stealing algorithm (default).
             *
             * @return tuner& reference to the tuner object.
             */
            tuner& set_static()
            {
                _static = true;
                return *this;
            }

            /**
             * Set the parallelization algorithm to dynamic work stealing (default)
             *
             * Qualcomm HETCOMPUTE implements a highly efficient work-stealing algorithm and uses it
             * as the common backend for parallel iteration patterns. It works well
             * with most workload types, especially for uneven workload distribution
             * across iterations. To improve performance further, consider tuning chunk
             * size (set_chunk_size) and degree of concurrency (set_max_doc).
             *
             * @sa     set_chunk_size(size_t)
             * @sa     set_max_doc(size_t)
             * @return tuner& reference to the tuner object.
             */

            tuner& set_dynamic()
            {
                _static = false;
                return *this;
            }

            /**
             * Check if the parallelization algorithm is static chunking.
             *
             * @return bool TRUE if using static chunking and false if using dynamic work stealing.
             */
            bool is_static() const { return _static; }

            /// @cond
            // TODO: implement shape setting in work stealing
            tuner& set_shape(const hetcompute::pattern::shape& s)
            {
                _shape = s;
                return *this;
            }

            hetcompute::pattern::shape get_shape() const { return _shape; }
            /// @endcond

            /**
             * Execute pattern sequentially
             *
             * @return tuner& reference to the tuner object.
             */
            tuner& set_serial()
            {
                _serialize = true;
                return *this;
            }

            /**
             * Check if pattern execution is serialized
             *
             * @return bool TRUE if execution is serialized and false otherwise.
             */
            bool is_serial() const { return _serialize; }

            /**
             * For patterns executable heterogeneously on multiple devices
             * (e.g. CPU, GPU, DSP), set fraction of pattern work to execute on the CPU.
             *
             * @param load = number of units out of
             *               total work (cpu_load + gpu_load + dsp_load)
             *               to execute on the CPU.
             *
             * return tuner& reference to the tuner object.
             */
            tuner& set_cpu_load(load_type load)
            {
                _cpu_load = load;
                return *this;
            }

            /**
             * For patterns executable heterogeneously on multiple devices
             * (e.g. CPU, GPU, DSP), get fraction of pattern work to be executed on the CPU.
             *
             * @return number of units out of total work (cpu_load + gpu_load + dsp_load)
             *         to be executed on the CPU
             */
            load_type get_cpu_load() const { return _cpu_load; }

            /**
             * For patterns executable heterogeneously on multiple devices
             * (e.g. CPU, GPU, DSP), set fraction of pattern work to execute on the DSP.
             *
             * @param load = number of units out of
             *               total work (cpu_load + gpu_load + dsp_load)
             *               to execute on the DSP.
             *
             * return tuner& reference to the tuner object.
             */
            tuner& set_dsp_load(load_type load)
            {
                _dsp_load = load;
                return *this;
            }

            /**
             * For patterns executable heterogeneously on multiple devices
             * (e.g. CPU, GPU, DSP), get fraction of pattern work to be executed on the DSP.
             *
             * @return number of units out of total work (cpu_load + gpu_load + dsp_load)
             *         to be executed on the DSP
             */
            load_type get_dsp_load() const { return _dsp_load; }

            /**
             * For patterns executable heterogeneously on multiple devices
             * (e.g. CPU, GPU, DSP), set fraction of pattern work to execute on the GPU.
             *
             * @param load = number of units out of
             *               total work (cpu_load + gpu_load + dsp_load)
             *               to execute on the GPU.
             *
             * return tuner& reference to the tuner object.
             */
            tuner& set_gpu_load(load_type load)
            {
                _gpu_load = load;
                return *this;
            }

            /**
             * For patterns executable heterogeneously on multiple devices
             * (e.g. CPU, GPU, DSP), set fraction of pattern work to execute on the GPU.
             */
            load_type get_gpu_load() const { return _gpu_load; }

            /**
             * Enable profiling within pattern execution.
             * Currently meaningful to hetero pfor_each pattern to generate
             * auto-tuned work distribution across heterogeneous devices.
             *
             * @return tuner& reference to the tuner object.
             */
            tuner& set_profile()
            {
                _profile = true;
                return *this;
            }

            /**
             * Check if hetero pfor_each pattern is profiled.
             *
             * @return bool TRUE if execution is profiled and false otherwise.
             */
            bool has_profile() const { return _profile; }

        private:
            size_t    _max_doc;
            size_t    _min_chunk_size;
            bool      _static;
            shape     _shape;
            bool      _serialize;
            bool      _user_setbit;
            load_type _cpu_load;
            load_type _dsp_load;
            load_type _gpu_load;
            bool      _profile;
        };

        /** @} */ /* end_addtogroup pattern_tuner_doc */

    }; // namespace pattern
};     // namespace hetcompute
