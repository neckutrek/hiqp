/*!
 * \file   hiqp_utils.h
 * \Author Marcus A Johansson (marcus.adam.johansson@gmail.com)
 * \date   July, 2016
 * \brief  Brief description of file.
 *
 * Detailed description of file.
 */



#ifndef HIQP_UTILS_H
#define HIQP_UTILS_H

// STL Includes
#include <iostream>

// Orocos KDL Includes
#include <kdl/tree.hpp>
#include <kdl/framevel.hpp>
#include <kdl/jntarray.hpp>
#include <kdl/jntarrayvel.hpp>
#include <kdl/jacobian.hpp>


// Eigen Includes
#include <Eigen/Dense>





namespace hiqp
{

std::ostream& operator<<(std::ostream& os, const KDL::Tree& kdl_tree);
std::ostream& operator<<(std::ostream& os, const KDL::FrameVel& kdl_frame_vel);
std::ostream& operator<<(std::ostream& os, const KDL::JntArrayVel& kdl_joints_vel);
std::ostream& operator<<(std::ostream& os, const KDL::Chain& kdl_chain);

unsigned int kdl_getQNrFromJointName
(
    const KDL::Tree& kdl_tree, 
    const std::string& joint_name
);

int kdl_JntToJac
(
    const KDL::Tree& tree,
    const KDL::JntArrayVel& qqdot, 
    KDL::Jacobian& jac, 
    const std::string& segmentname
);













/*!
 *  \brief calculates the Moore-Penrose Pseudoinverse for any sized matrices.
 *
 *  The original source code is got from http://eigendobetter.com/, I edited it 
 *  to be compilable in this form. /neckutrek
 *
 *  \param a : the matrix to be inverted
 *
 *  \return the inverted matrix
 */
template<typename Derived>
Derived pinv(const Eigen::MatrixBase<Derived>& a)
{
    typedef typename Eigen::MatrixBase<Derived>::RealScalar RealScalar;
    if (a.rows() < a.cols())
    {
        auto svd = a.derived().transpose().jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV);
        RealScalar tolerance = (RealScalar)std::numeric_limits<RealScalar>::epsilon() * std::max((RealScalar)a.cols(), (RealScalar)a.rows()) * svd.singularValues().array().abs().maxCoeff();
        return (svd.matrixV() * Derived((svd.singularValues().array().abs() > tolerance).select(svd.singularValues().array().inverse(), 0)).asDiagonal() * svd.matrixU().adjoint()).transpose();
    }
    Eigen::JacobiSVD<Derived> svd = a.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV);
    RealScalar tolerance = (RealScalar)std::numeric_limits<RealScalar>::epsilon() * std::max((RealScalar)a.cols(), (RealScalar)a.rows()) * svd.singularValues().array().abs().maxCoeff();
    return svd.matrixV() * Derived((svd.singularValues().array().abs() > tolerance).select(svd.singularValues().array().inverse(), 0)).asDiagonal() * svd.matrixU().adjoint();
}




/*!
 *  \brief calculates the Damped-Least-Square matrix.
 *
 *  \param a : the matrix to be inverted
 *
 *  \return the damped-least-square matrix on success, 
 *          the given matrix is returned otherwise.
 */
template<typename Derived>
Derived dls
(
    const Eigen::MatrixBase<Derived>& a, 
    double eta = 0.01
)
{
    unsigned int r = a.rows();
    unsigned int c = a.cols();

    if (r > c) 
        return a;

    if (r == c)
        return pinv(a);

    Derived a_ext(r+c, c);
    a_ext.block(0, 0, r, c) = a;
    a_ext.block(r, 0, c, c) = eta * Eigen::MatrixXd::Identity(c, c);
    Derived b = Eigen::MatrixXd::Zero(r+c, r);
    b.block(0, 0, r, r) = Eigen::MatrixXd::Identity(r, r);

    return pinv(a_ext) * b;
}










} // namespace hiqp






#endif