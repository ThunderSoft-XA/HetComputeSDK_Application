#ifndef HETCOMPUTE_LIBPOWER_AFFINITY_H
#define HETCOMPUTE_LIBPOWER_AFFINITY_H

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup thread_affinity_doc
@{ */
/**
 * C Affinity API
 * @sa include/hetcompute/affinity.hh for the C++ Affinity API
 */

typedef enum {
    hetcompute_affinity_cores_all = 0, /** Use all SoC cores to set the affinity */
    hetcompute_affinity_cores_big,     /** Set all threads to be eligible to run in the big cluster of the SoC */
    hetcompute_affinity_cores_little   /** Set all threads to be eligible to run in the LITTLE cluster of the SoC */
} hetcompute_affinity_cores_t;

typedef enum {
    hetcompute_affinity_mode_allow_local_setting = 0,
    /** Set a default affinity for all cpu tasks for which
      affinity was not specified. For example, if the user
      sets the affinity in allow_local_setting mode to big,
      the big cores will execute all tasks except those
      marked as little */
    hetcompute_affinity_mode_override_local_setting,
    /** Set the affinity for all cpu tasks regardless of
      local task/scope settings. For example, if the user
      sets the affinity to little in
      override_local_setting mode, the little cores will
      execute all cpu tasks including those marked as big
      */
} hetcompute_affinity_mode_t;

typedef enum {
    hetcompute_affinity_pin_threads_false = 0, /** Do not pin threads to individual cores */
    hetcompute_affinity_pin_threads_true       /** Pin threads to individual cores */
} hetcompute_affinity_pin_threads_t;

typedef struct
{
    hetcompute_affinity_cores_t       cores;       /** Group of cores to set the affinity to */
    hetcompute_affinity_pin_threads_t pin_threads; /** Pin threads to individual cores */
    hetcompute_affinity_mode_t        mode;        /** Mode to set the affinity */
} hetcompute_affinity_settings_t;

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
void hetcompute_affinity_set(const hetcompute_affinity_settings_t as);

/**
  Reset the thread pool affinity so that all Qualcomm HetCompute threads can run in any core of
  the system.
*/
void hetcompute_affinity_reset();

/**
 * Return the current affinity settings.
 */

hetcompute_affinity_settings_t hetcompute_affinity_get();

/**
 * Function pointer type to pass to hetcompute_affinity_execute()
 * @sa hetcompute_affinity_execute()
 */
typedef void (*hetcompute_func_ptr_t)(void* data);

/**
 * Execute function with args, while enforcing desired affinity
 * (big/LITTLE/all).
 *
 * @note Call returns upon completion of function f
 *
 * @param desired_aff desired affinity for execution
 * @param f function to execute
 * @param args arguments to pass to function f for execution
 */
void hetcompute_affinity_execute(const hetcompute_affinity_settings_t desired_aff, hetcompute_func_ptr_t f, void* args);

/** @} */ /* end_addtogroup thread_affinity_doc */

#ifdef __cplusplus
}
#endif

#endif // HETCOMPUTE_LIBPOWER_AFFINITY_H
