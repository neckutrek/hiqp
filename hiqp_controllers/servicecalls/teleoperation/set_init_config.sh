rosservice call /yumi/hiqp_kinematics_controller/set_task \
"name: 'task_teleop_init'
priority: 3
visible: 1
active: 1
def_params: ['TDefFullPose', '-1.8', '0.26', '-1.15', '0.72', '0.0', '0.0', '0.0', '0', '0', '0.42', '-1.48', '-1.21', '0.75', '0.8', '0.45', '1.21', '0', '0']
dyn_params: ['TDynFirstOrder', '5']"
