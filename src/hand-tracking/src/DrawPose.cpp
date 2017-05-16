#include <utility>

#include "DrawPose.h"

using namespace bfl;
using namespace Eigen;


DrawPose::DrawPose(std::unique_ptr<StateModel> transition_model) noexcept :
    state_model_(std::move(transition_model)) { }


DrawPose::~DrawPose() noexcept { }


DrawPose::DrawPose(DrawPose&& pf_prediction) noexcept :
    state_model_(std::move(pf_prediction.state_model_)) { };


DrawPose& DrawPose::operator=(DrawPose&& pf_prediction) noexcept
{
    state_model_ = std::move(pf_prediction.state_model_);

    return *this;
}


void DrawPose::motion(const Ref<const VectorXf>& cur_state, Ref<VectorXf> prop_state)
{
    state_model_->propagate(cur_state, prop_state);
}


void DrawPose::motionDisturbance(Ref<VectorXf> sample)
{
    state_model_->noiseSample(sample);
}


void DrawPose::predict(const Ref<const VectorXf>& prev_state, Ref<VectorXf> pred_state)
{
    motion(prev_state, pred_state);

    VectorXf sample(VectorXf::Zero(7, 1));
    motionDisturbance(sample);

    VectorXf rotated_vec(VectorXf::Zero(3, 1));
    addAxisangleDisturbance(pred_state.tail<3>(), sample(6), sample.middleRows<3>(3), rotated_vec);

    pred_state.head<3>() += sample.head<3>();
    pred_state.tail<3>() =  rotated_vec;
}

bool DrawPose::setStateModelProperty(const std::string& property)
{
    return state_model_->setProperty(property);
}


void DrawPose::addAxisangleDisturbance(const Ref<const Vector3f>& current_vec, float disturbance_angle, const Ref<const Vector3f>& disturbance_vec, Ref<Vector3f> rotated_vec)
{
    float ang = current_vec.tail<3>().norm() + disturbance_angle;

    if      (ang >  2.0 * M_PI) ang -= 2.0 * M_PI;
    else if (ang <=        0.0) ang += 2.0 * M_PI;

    /* Find the rotation axis 'u' and rotation angle 'rot' [1] */
    Vector3f def_dir(0.0, 0.0, 1.0);
    Vector3f u = def_dir.cross(current_vec.normalized()).normalized();
    float rot  = static_cast<float>(acos(current_vec.normalized().dot(def_dir)));

    /* Convert rotation axis and angle to 3x3 rotation matrix [2] */
    /* [2] https://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle */
    Matrix3f cross_matrix;
    cross_matrix <<     0,  -u(2),   u(1),
                     u(2),      0,  -u(0),
                    -u(1),   u(0),      0;
    Matrix3f R = cos(rot) * Matrix3f::Identity() + sin(rot) * cross_matrix + (1 - cos(rot)) * (u * u.transpose());

    rotated_vec = ang * (R * disturbance_vec).normalized();
}