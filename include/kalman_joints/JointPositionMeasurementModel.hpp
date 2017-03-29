#pragma once
#include <kalman/LinearizedMeasurementModel.hpp>
#include <kalman_joints/JointTypes.hpp>
#include <kalman_joints/NumericalDiff.hpp>

namespace Joints
{
/**
 * @brief Measurement model for measuring joint positions
 *
 *  This is a measurement model for measuring joint position from
 *  absolute encoders observations.
 *
 * @param T Numeric scalar type
 * @param N Numer of joints
 * @param CovarianceBase Class template to determine the covariance representation
 *                       (as covariance matrix (StandardBase) or as lower-triangular
 *                       coveriace square root (SquareRootBase))
 */
template <typename T, unsigned int N, template <class> class CovarianceBase = Kalman::StandardBase>
class JointPositionMeasurementModel : public Kalman::LinearizedMeasurementModel<State<T, N>, JointMeasurement<T, N>, CovarianceBase>
{
   public:
    //! State type shortcut definition
    using S = State<T, N>;

    //! Measurement type shortcut definition
    using M = JointMeasurement<T, N>;

    using MeasurementJacobian = Kalman::Jacobian<T, S, M>;

    // Wraps the system model cost function in an automatic differentiation functor
    template <typename Parent>
    struct Functor_ : Functor<T>
    {
        Parent* m;

        Functor_(Parent* m) : m(m)
        {
        }

        int operator()(const Eigen::VectorXd& x, Eigen::VectorXd& fvec) const
        {
            S s;
            s.q(x);
            M o;
            // Cost function
            m->measure(s, o);
            fvec = o;
            return 0;
        }

        int inputs() const { return N; }  // There are two parameters of the model
        int values() const { return N; }  // The number of observations
    };
    using Functor = Functor_<JointPositionMeasurementModel<T, N>>;
    NumericalDiffFunctor<Functor> num_diff;

    JointPositionMeasurementModel() : num_diff(this)
    {
        // Setup noise jacobian. As this one is static, we can define it once
        // and do not need to update it dynamically
        this->V.setIdentity();
    }

    /**
     * @brief Definition of (possibly non-linear) measurement function
     *
     * This function maps the system state to the measurement that is expected
     * to be received from the sensor assuming the system is currently in the
     * estimated state.
     * Here we are only interested in the pose measurement (position+orientation)
     *
     * @param [in] x The system state in current time-step
     * @param [out] out The (predicted) measurement for the system state
     */
    void measure(const S& x, M& out)
    {
        out = x.q();
    }

    /**
     * @brief Update jacobian matrices for the system state transition function using current state
     *
     * This will re-compute the (state-dependent) elements of the jacobian matrices
     * to linearize the non-linear measurement function \f$h(x)\f$ around the
     * current state \f$x\f$.
     *
     * @note This is only needed when implementing a LinearizedSystemModel,
     *       for usage with an ExtendedKalmanFilter or SquareRootExtendedKalmanFilter.
     *       When using a fully non-linear filter such as the UnscentedKalmanFilter
     *       or its square-root form then this is not needed.
     *
     * @param x The current system state around which to linearize
     * @returns J The measurement jacobian
     */
    MeasurementJacobian getJacobian(const S& x)
    {
        // H = dh/dx (Jacobian of measurement function w.r.t. the state)
        MeasurementJacobian J;
        // 6x12 jacobian for the pose measurement update
        Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> jac(J.rows(), J.cols());
        jac.setZero();
        Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> xx = x.q();
        num_diff.df(xx, jac);
        J = jac;
        // std::cout << "jacobian pose: \n" << J << std::endl;
        return J;
    }

    DEPRECATED M h(const S& x) const override
    {
    }

   protected:
    DEPRECATED void updateJacobians(const S& x)
    {
    }
};

}  // namespace Robot