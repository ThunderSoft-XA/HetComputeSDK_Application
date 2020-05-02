/** @file affinity.hh */
#pragma once

// Include user-visible headers
#include <hetcompute/taskfactory.hh>

namespace hetcompute
{
    namespace affinity
    {
        /** @addtogroup thread_affinity_doc
        @{ */

        /**
         * C++ Affinity API
         * @sa include/hetcompute/affinity.h for the C Affinity API
         */

        /** Enumeration type to select the cores where to apply affinity settings
         *  in a big-little system. In homogeneous systems, all is always used.
         */
        enum class cores
        {
            all,   /**< Use all SoC cores to set the affinity */
            big,   /**< Set all threads to be eligible to run in the big cluster of the SoC */
            little /**< Set all threads to be eligible to run in the big cluster of the SoC */
        };

        /** Enumeration type to select the affinity mode in big-little systems. In homogeneous system,
         *  mode, as cores, is ignored.
         */
        enum class mode
        {
            allow_local_setting,    /**< Set a default affinity for all cpu tasks for which
                                      affinity was not specified. For example, if the user
                                      sets the affinity in allow_local_setting mode to big,
                                      the big cores will execute all tasks except those
                                      marked as little */
            override_local_setting, /**< Set the affinity for all cpu tasks regardless of
                                      local task/scope settings. For example, if the user
                                      sets the affinity to little in
                                      override_local_setting mode, the little cores will
                                      execute all cpu tasks including those marked as big
                                      */
        };

        /** Affinity settings class
         *
         * This class is used to define the affinity conditions desired by the programmer
         */
        class settings
        {
        public:
            /** Constructor with cores and pin_threads arguments
             *
             * @param[in] cores_attribute Type of cores
             *                            hetcompute::affinity::cores::all
             *                            hetcompute::affinity::cores::big
             *                            hetcompute::affinity::cores::little
             *
             * @param[in] pin_threads If true, enable pinning
             *
             * @param[in] md Operation mode
             *               hetcompute::affinity::mode::allow_local_setting
             *               hetcompute::affinity::mode::override_local_setting
             */
            explicit settings(cores cores_attribute, bool pin_threads, mode md = ::hetcompute::affinity::mode::allow_local_setting)
                : _cores(cores_attribute), _pin_threads(pin_threads), _mode(md)
            {
            }
            /** Destructor
             */
            ~settings() {}

            /** Return the cores affinity member
             */
            cores get_cores() const { return _cores; }

            /** Set the cores affinity member
             *
             * @param[in] cores_attribute Type of desired cores.
             */
            void set_cores(cores cores_attribute) { _cores = cores_attribute; }

            /** Return the pin affinity member
             *
             * @return
             * <code>true</code>--Device threads are pinned.\n
             * <code>false</code>--Device threads are not pinned.
             */
            bool get_pin_threads() const { return _pin_threads; }

            /** Set the pin affinity member
             *
             * When the pin is set, each hetcompute thread will be pinned to a single core
             */
            void set_pin_threads() { _pin_threads = true; }

            /** Reset the pin affinity member
             */
            void reset_pin_threads() { _pin_threads = false; }

            /** Return the mode member
             *
             */
            mode get_mode() const { return _mode; }

            /** Set the mode member
             *
             * When mode is force, all task will be executed by the CPUs set by the
             * current affinity settings. When normal, cpu tasks will be run by default
             * by the same set of CPUs, except when the user has manually set the kernel
             * affinity with set_big() or set_little().
             */
            void set_mode(mode md) { _mode = md; }

            /** Equality operator to compare settings object
             *
             * @return
             * <code>true</code>--Compared settings are equal.\n
             * <code>false</code>--Compared settings are different.
             */
            bool operator==(const settings& rhs) const
            {
                return (_cores == rhs._cores) && (_pin_threads == rhs._pin_threads) && (_mode == rhs._mode);
            }

            /** Inequality operator to check for different settings objects
             *
             * @return
             * <code>true</code>--Compared settings are different.\n
             * <code>false</code>--Compared settings are equal.
             */
            bool operator!=(const settings& rhs) const { return !(*this == rhs); }

        private:
            cores _cores;       /**< Group of cores to set the affinity to */
            bool  _pin_threads; /**< Pin thread to individual cores */
            mode  _mode;        /**< Mode to set the affinity */
        };

        /**
          * Set the affinity of all Qualcomm HetCompute runtime pool threads.
          *
          * A successful call to this function will set the affinity of all runtime
          * threads to the requested settings.

          * The HetCompute affinity settings enables to control three knobs:
          *   - cores: Set where to run the task.
          *   - pinning: Set whether threads can migrate among cores
          *   - mode: Set whether kernel affinity attributes are going to be fulfilled.
          *           When mode equals normal, set_big() and set_little() kernel attributes
          *           are respected, when mode equals force, otherwise are ignored.
          *           Force mode can be useful in situations where the
          *           programmers want to guarantee that certain cores are not used; e.g.; leave
          *           the little cores for an audio library in a game. The default mode for HetCompute is normal.

          * For example, to run all Qualcomm HetCompute threads in all big cores of the SoC
          * allowing kernel attributes, call this function with a settings object with
          * cores, pin, and mode equal to big, false, and normal respectively. Or to
          * enable pinning in a system with homogeneous cores, call with all and true.
          * This will set the mode to normal, since it is the default.

          * @param[in] as Affinity settings object.

          * @par Example 1 Setting the affinity with force and normal modes
          * @includelineno samples/src/affinity.cc
        */

        void set(const settings as);

        /**
          Reset the thread pool affinity so that all Qualcomm HetCompute threads can run in any core of
          the system.
        */
        void reset();

        /**
         * Return the current affinity settings.
         */

        settings get();

    }; // namespace affinity
};     // namespace hetcompute

namespace hetcompute
{
    namespace internal
    {
        namespace affinity
        {
            hetcompute::affinity::settings get_non_local_affinity_settings();
        }; // namespace affinity

        namespace soc
        {
            /**
             * Is the core on which the calling thread is running a big core?
             * @return true if big core, false if LITTLE core
             */
            bool is_this_big_core();
            bool is_big_little_cpu();

        }; // namespace soc
    };     // namespace internal
};         // namespace hetcompute

namespace hetcompute
{
    namespace affinity
    {
        /**
         * Execute function/lambda/function-object with args, while enforcing desired
         * affinity (big/LITTLE/all).
         *
         * @note Call returns upon completion of function f
         *
         * @param desired_aff desired affinity for execution
         * @param f function to execute; may be lambda/function/function-object
         * @param args arguments to pass to function f for execution
         */
        template <typename Function, typename... Args>
        void execute(hetcompute::affinity::settings desired_aff, Function&& f, Args... args)
        {
            if (hetcompute::internal::soc::is_big_little_cpu())
            {
                auto desired_cores      = desired_aff.get_cores();
                auto non_local_settings = hetcompute::internal::affinity::get_non_local_affinity_settings();

                if (non_local_settings.get_mode() == hetcompute::affinity::mode::override_local_setting)
                {
                    if (non_local_settings.get_cores() == hetcompute::affinity::cores::all)
                    {
                        // run anywhere since mode is forced to be all
                        f(args...);
                        return;
                    }
                    else
                    {
                        desired_cores = non_local_settings.get_cores();
                    }
                }
                else
                {
                    // local settings are not to be overridden
                }

                auto is_caller_executing_on_big = hetcompute::internal::soc::is_this_big_core();

                // If caller is executing on a cluster different from the desired one, then
                // ship function off to the desired cluster using a task.
                auto diff_aff = (((desired_cores == hetcompute::affinity::cores::big) && !is_caller_executing_on_big) ||
                                 ((desired_cores == hetcompute::affinity::cores::little) && is_caller_executing_on_big));
                if (diff_aff)
                {
                    auto ck = hetcompute::create_cpu_kernel(f);
                    if (desired_cores == hetcompute::affinity::cores::big)
                    {
                        ck.set_big();
                    }
                    else if (desired_cores == hetcompute::affinity::cores::little)
                    {
                        ck.set_little();
                    }
                    auto t = hetcompute::launch(ck, args...);
                    t->wait_for();
                    return;
                }
            }
            // TODO_biglittle: Even though the current thread may be running on the
            // desired cluster, it may migrate. Should we pin the current thread?
            f(args...);
            return;
        }

        /** @} */ /* end_addtogroup thread_affinity_doc */

    }; // namespace affinity

}; // namespace hetcompute
