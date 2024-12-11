//   Copyright 2024 Filippo Savi <filssavi@gmail.com>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.



#include <gtest/gtest.h>
#include "deployment/custom_deployer.hpp"
#include "emulator/emulator_manager.hpp"

#include "../deployment/fpga_bridge_mock.hpp"


nlohmann::json custom_deployer_get_addr_map(){

    uint64_t cores_rom_base = 0x5'0000'0000;
    uint64_t cores_rom_offset = 0x1000'0000;

    uint64_t cores_control_base = 0x4'43c2'0000;
    uint64_t cores_control_offset =    0x1'0000;

    uint64_t cores_inputs_base_address = 0x2000;
    uint64_t cores_inputs_offset = 0x1000;

    uint64_t dma_offset =   0x1000;

    uint64_t controller_base =   0x4'43c1'0000;
    uint64_t controller_tb_offset =   0x1000;
    uint64_t scope_mux_base = 0x4'43c5'0000;

    uint64_t hil_control_base = 0x4'43c0'0000;
    nlohmann::json addr_map, offsets, bases;
    bases["cores_rom"] = cores_rom_base;
    bases["cores_control"] = cores_control_base;
    bases["cores_inputs"] = cores_inputs_base_address;
    bases["controller"] = controller_base;

    bases["scope_mux"] = scope_mux_base;
    bases["hil_control"] = hil_control_base;


    offsets["cores_rom"] = cores_rom_offset;
    offsets["cores_control"] =cores_control_offset;
    offsets["controller"] = controller_tb_offset;
    offsets["cores_inputs"] = cores_inputs_offset;
    offsets["dma"] = dma_offset;
    offsets["hil_tb"] = 0;
    addr_map["bases"] = bases;
    addr_map["offsets"] = offsets;
    return addr_map;
}


TEST(custom_deployer, deployment) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "cores": [
        {
            "id": "vsi",
            "order": 1,
            "input_data": [],
            "inputs": [
                {
                    "name": "sin_t",
                    "metadata": {
                        "type": "integer",
                        "width": 14,
                        "signed": false,
                        "common_io":true
                    },
                    "source": {
                        "type": "external",
                        "value": ""
                    },
                    "reg_n": [
                        1
                    ],
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "cos_t",
                    "metadata": {
                        "type": "integer",
                        "width": 10,
                        "signed": true,
                        "common_io":true
                    },
                    "source": {
                        "type": "external",
                        "value": ""
                    },
                    "reg_n": [
                        2
                    ],
                    "channel": 0
                },
                {
                    "name": "v_in",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io":true
                    },
                    "source": {
                        "type": "external",
                        "value": ""
                    },
                    "reg_n": [
                        3
                    ],
                    "channel": 0
                },
                {
                    "name": "i_out",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io":true
                    },
                    "source": {
                        "type": "external",
                        "value": ""
                    },
                    "reg_n": [
                        4,
                        5,
                        6
                    ],
                    "channel": 0
                },
                {
                    "name": "i_d_ref",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io":true
                    },
                    "source": {
                        "type": "external",
                        "value": ""
                    },
                    "reg_n": [
                        7
                    ],
                    "channel": 0
                },
                {
                    "name": "i_q_ref",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io":true
                    },
                    "source": {
                        "type": "external",
                        "value": ""
                    },
                    "reg_n": [
                        8
                    ],
                    "channel": 0
                }
            ],
            "outputs": [
                {
                    "name": "v_out",
                    "type": "float",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io":true
                    },
                    "reg_n": [
                        10,
                        11,
                        12
                    ]
                },
                {
                    "name": "i_in",
                    "type": "float",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io":true
                    },
                    "reg_n": [
                        13
                    ]
                }
            ],
            "memory_init": [
                {
                    "name": "d_integrator",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false
                    },
                    "is_output": false,
                    "reg_n": [
                        14
                    ],
                    "value": [
                        0
                    ]
                },
                {
                    "name": "q_integrator",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false
                    },
                    "is_output": false,
                    "reg_n": [
                        15
                    ],
                    "value": [
                        0
                    ]
                }
            ],
            "deployment": {
                "control_address": 18316591104,
                "has_reciprocal": true,
                "rom_address": 21474836480
            },
            "channels": 1,
            "program": {
                "content": "void main(){\n  \n//PARAMETERS\nfloat kp = 0.8; \nfloat ki = 250.0;\nfloat ts = 1/20e3;\nfloat pp = 4.0;\nfloat dt = 1e-6;\n  \n// MEMORIES\nfloat d_integrator;\nfloat q_integrator;\n//INPUTS\nfloat i_d_ref;\nfloat i_q_ref;\nfloat i_out[3];\nfloat angle;\nfloat v_in;\n//OUTPUTS\nfloat v_out [3];\nfloat i_in;\n    \n    float v_d;\n    float v_q;\n  \n    float sat_p = 1.0;\n    float sat_n = -1.0;\n\n    float v_dq[2];\n    float i_dq[2];\n  \n    abc_dq(i_out, i_dq, cos_t, sin_t);\n\n    float d_ref = itf(i_d_ref)/100.0;\n    float d_err = d_ref - i_dq[0];\n    v_dq[0] = pi(d_err, kp, ki, ts, sat_p,  sat_n, d_integrator);\n\n  \n    float q_ref = itf(i_q_ref)/100.0;\n    float q_err = q_ref - i_dq[1] - d_ref*0.37;\n    v_dq[1] = pi(q_err, kp, ki, ts, sat_p,  sat_n, q_integrator);\n\n    float duty[3];\n   dq_abc(v_dq, duty, cos_t, sin_t);\n    \n    float v_max = v_in*0.5;\n    \n    float dt_err = v_in*dt*20e3;\n\n    float v_out_a =  apply_dt_effect(duty[0]*v_max, dt_err);\n    float v_out_b =  apply_dt_effect(duty[1]*v_max, dt_err);\n    float v_out_c =  apply_dt_effect(duty[2]*v_max, dt_err);\n    \n    v_out[0] = v_out_a;\n    v_out[1] = v_out_b;\n    v_out[2] = v_out_c;\n  \n    float p_out = v_out[0]*i_out[0] + v_out[1]*i_out[1] + v_out[2]*i_out[2];\n     i_in = p_out/v_in;\n}",
                "build_settings": {
                    "io": {
                        "inputs": [
                            "v_in",
                            "i_d_ref",
                            "i_q_ref",
                            "i_out",
                            "sin_t",
                            "cos_t"
                        ],
                        "memories": [
                            "d_integrator",
                            "q_integrator"
                        ],
                        "outputs": [
                            "v_out",
                            "i_in"
                        ]
                    }
                },
                "headers": [
                    "////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n//                                    TRIGONOMETRIC FCNS                               //\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\n\nfloat sin(float theta){\n    float efi_in[2];\n    \n    efi_in[0] = 0;\n    efi_in[1] = theta;\n\n    float sin_theta = efi(efi_in, 2);\n    sin_theta = itf(sin_theta);\n    sin_theta = 0.000030518*sin_theta;\n\n    return sin_theta;\n} \n\nfloat cos(float theta){\n    \n    float efi_in[2];\n    \n    efi_in[0] = 1;\n    efi_in[1] = theta;\n\n    float cos_theta = efi(efi_in, 2);\n\n    cos_theta = itf(cos_theta);\n    cos_theta = 0.000030518*cos_theta;\n    return cos_theta;\n}\n\nfloat sign(float in) {\n    return in > 0.0 ? 1.0 : -1.0;\n}\n\nfloat sat(float in, float sat_p, float sat_n){\n  float intermediate = satp(in, sat_p);\n  return satn(intermediate, sat_n);\n}\n\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n//                                         TRANSFORMATIONS                               //\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\nvoid clarke(float in[3], float out[2]){\n  \n    float sqrt3_2 = 0.86602540378;\n  \n    out[0] = 2.0/3.0*(in[0] - in[1]*0.5 - in[2]*0.5);\n    out[1] = 2.0/3.0*sqrt3_2*(in[1] - in[2]);\n}\n\nvoid clarke_inv(float in[2], float out[3]){\n  \n    float sqrt3_2 = 0.86602540378;\n\n    out[0] = in[0];\n    out[1] = - in[0]*0.5 + sqrt3_2*in[1];\n    out[2] = - in[0]*0.5 - sqrt3_2*in[1];\n}\n\nvoid park(float in[2], float out[2], float cos_t, float sin_t){\n  \n    //park\n    out[0] = in[0]*cos_t + in[1]*sin_t;\n    out[1] = -in[0]*sin_t + in[1]*cos_t;\n}\n\nvoid park_inv(float in[2], float out[2], float cos_t, float sin_t){\n  \n    //park\n    out[0] =  in[0]*cos_t - in[1]*sin_t;\n    out[1] =  in[0]*sin_t + in[1]*cos_t;\n}\n\nvoid abc_dq(float in[3], float out[2], float cos_t, float sin_t){\n    float ab[2];\n    clarke(in, ab);\n    park(ab, out, cos_t, sin_t);\n}\n\nvoid dq_abc(float in[2], float out[3], float cos_t, float sin_t){\n    float ab_inv[2];\n    park_inv(in,  ab_inv,  cos_t, sin_t);\n    clarke_inv(ab_inv, out);\n}\n\n\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n//                                         CONTROLLERS                                        //\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\nfloat pi(float err, float kp, float ki, float ts, float sat_p, float sat_n, float integrator ){\n  \n     float action_unsat = kp*err + integrator;\n\n    integrator = ki*ts*err + integrator;\n\n    integrator = sat(integrator,sat_p,  sat_n);\n    return sat(action_unsat, sat_p, sat_n);\n}\n\nfloat apply_dt_effect(float in, float dt_err){\n      float s  = sign(in);\n      float dt_effect = dt_err*s;\n     return in - dt_effect;\n}"
                ]
            },
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "sampling_frequency": 0
        },
        {
            "id": "machine",
            "order": 2,
            "input_data": [],
            "inputs": [
                {
                    "name": "voltages",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io":true
                    },
                    "source": {
                        "type": "external",
                        "value": ""
                    },
                    "reg_n": [
                        1,
                        2,
                        3
                    ],
                    "channel": [
                        0
                    ]
                }
            ],
            "outputs": [
                {
                    "name": "currents",
                    "type": "float",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false
                    },
                    "reg_n": [
                        4,
                        5,
                        6
                    ]
                },
                {
                    "name": "speed",
                    "type": "float",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false
                    },
                    "reg_n": [
                        7
                    ]
                },
                {
                    "name": "sin_t",
                    "type": "integer",
                    "metadata": {
                        "type": "integer",
                        "width": 16,
                        "signed": true
                    },
                    "reg_n": [
                        8
                    ]
                },
                {
                    "name": "cos_t",
                    "type": "integer",
                    "metadata": {
                        "type": "integer",
                        "width": 14,
                        "signed": false
                    },
                    "reg_n": [
                        9
                    ]
                }
            ],
            "memory_init": [
                {
                    "name": "i_d",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false
                    },
                    "is_output": false,
                    "reg_n": [
                        22
                    ],
                    "value": [
                        0
                    ]
                },
                {
                    "name": "i_q",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false
                    },
                    "is_output": false,
                    "reg_n": [
                        23
                    ],
                    "value": [
                        0
                    ]
                },
                {
                    "name": "theta_m",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false
                    },
                    "is_output": false,
                    "reg_n": [
                        24
                    ],
                    "value": [
                        0
                    ]
                }
            ],
            "deployment": {
                "control_address": 18316656640,
                "has_reciprocal": false,
                "rom_address": 21743271936
            },
            "channels": 2,
            "program": {
                "content": "float calc_id(\n        float i_d,\n        float i_q,\n        float Ld,\n        float Lq,\n        float Rs,\n        float dt,\n        float dt2,\n        float omega_e,\n        float omega2,\n        float phi_m,\n        float v_dq[2]\n        ){\n\n    float idn_f1 = i_q*Lq*Lq*dt*omega_e;\n    float idn_f2 = phi_m*Lq*dt2*omega2;\n    float idn_f3 = v_dq[1]*Lq*dt2*omega_e;\n    float idn_f4 = v_dq[0]*Lq*dt;\n    float idn_f5 = i_d*Ld*Lq;\n    float idn_f6 = Rs*v_dq[0]*dt2;\n    float idn_f7 = i_d*Ld*Rs*dt;\n\n    return  idn_f1 - idn_f2 + idn_f3 + idn_f4 + idn_f5 + idn_f6 + idn_f7;\n}\n\nfloat calc_iq(\n        float i_d,\n        float i_q,\n        float Ld,\n        float Lq,\n        float Rs,\n        float dt,\n        float dt2,\n        float omega_e,\n        float omega2,\n        float phi_m,\n        float v_dq[2]\n){\n\n\n    float iqn_f1 = i_q*Ld*Lq;\n    float iqn_f2 = Ld*dt*v_dq[1];\n    float iqn_f3 = Rs*dt2*v_dq[1];\n    float iqn_f4 = i_d*Ld*Ld*dt*omega_e;\n    float iqn_f5 = Ld*dt2*v_dq[0]*omega_e;\n    float iqn_f6 = Rs*dt2*omega_e*phi_m;\n    float iqn_f7 = i_q*Lq*Rs*dt;\n    float iqn_f8 = Ld*dt*omega_e*phi_m;\n\n\n    return iqn_f1 + iqn_f2 + iqn_f3 - iqn_f4 - iqn_f5 - iqn_f6 + iqn_f7 - iqn_f8;\n    \n}\n\nvoid main(){\n\n    float r_mach = 0.901;\n    float r_cbl =  90e-3;\n    float r_on = 250e-3;\n    \n    float Ld = 0.006552;\n    float Lq = 0.006552;\n    float phi_m =  0.09427;\n    float pp  = 4.0;\n    float Ts  =1/20.0e3;\n    float J  = 1.2e-4;\n    float speed_rpm = 1600.0;\n    float den = 22944.5;\n  \n    //INPUTS\n    float voltages[3];\n    //MEMORIES\n    float  i_d;\n    float  i_q;\n    float angle;\n    float angle_n;\n    //OUTPUTS\n    float currents[3];\n    float omega_m;\n    float t_elec;\n\n    //constants\n    float pi2 = 6.283186;\n    float pi_over_2 = 1.57079632679;\n\n    float f_mech = speed_rpm/60.0;\n    float omega_m = pi_over_2*f_mech;\n    float theta_incr = omega_m*Ts;\n\n    float dt = Ts;\n    float dt2 = Ts*Ts;\n    float omega_e = omega_m*pp;\n    float omega2 = omega_e*omega_e;\n\n  \n    theta_m = (omega_m*Ts + theta_m);\n    float theta_mod = theta_m > 6.283186 ?  0.0 : theta_m;\n    theta_m = theta_mod;\n\n  \n    float shifted_scaled_angle = theta_m*pp/6.283186*65535.0;\n    int int_angle =  fti(shifted_scaled_angle);\n    int modulo_angle = int_angle & 65535;\n  \n  \n    float v_d, v_q;\n    float v_dq[2];\n    float i_dq[2];\n    float v_ab[2];\n\n\n\n        //clarke\n     v_ab[0] = 0.666666*(voltages[0] - voltages[1]*0.5 - voltages[2]*0.5);\n     v_ab[1] = 0.57734969183*(voltages[1] - voltages[2]);  \n\n    float sin_t =sin(modulo_angle);\n    float cos_t =cos(modulo_angle);\n\n    //park\n    v_dq[0] = v_ab[0]*cos_t + v_ab[1] *sin_t;\n    v_dq[1] = -v_ab[0]*sin_t + v_ab[1] *cos_t;\n\n\n    /*  \n    float sin_t =sin(modulo_angle);\n    float cos_t =cos(modulo_angle);\n\n    abc_dq(voltages, v_dq, cos_t, sin_t);\n  */\n  \n    //float Rs = r_mach + 2.0*r_cbl  + 2.0*r_on;\n    float Rs = 1.581;\n  \n    float id_nom = calc_id(i_d,i_q,Ld,Lq, Rs, dt,dt2,omega_e,omega2, phi_m, v_dq);\n    float iq_nom = calc_iq(i_d,i_q,Ld,Lq, Rs, dt,dt2,omega_e,omega2, phi_m, v_dq);\n         \n    v_d = v_dq[0]*1.0;\n    v_q = v_dq[1]*1.0;\n  \n    float ef = 1.0;\n  \n    i_d = id_nom*den*ef; // CONSTANT PRECOMPUTED FROM DENOMINATOR\n    i_q = iq_nom*den*ef; \n\n    i_dq[0] = i_d;\n    i_dq[1] = i_q;\n    \n     dq_abc(i_dq, currents, cos_t, sin_t);\n  \n    t_elec = 1.5*pp*(i_q*(i_d*Ld+phi_m)- i_d*i_q*Lq);\n\n}",
                "build_settings": {
                    "io": {
                        "inputs": [
                            "voltages"
                        ],
                        "memories": [
                            "i_d",
                            "i_q",
                            "angle_n",
                            "theta_m"
                        ],
                        "outputs": [
                            "speed",
                            "currents",
                            "sin_t",
                            "cos_t"
                        ]
                    }
                },
                "headers": [
                    "////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n//                                    TRIGONOMETRIC FCNS                               //\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\n\nfloat sin(float theta){\n    float efi_in[2];\n    \n    efi_in[0] = 0;\n    efi_in[1] = theta;\n\n    float sin_theta = efi(efi_in, 2);\n    sin_theta = itf(sin_theta);\n    sin_theta = 0.000030518*sin_theta;\n\n    return sin_theta;\n} \n\nfloat cos(float theta){\n    \n    float efi_in[2];\n    \n    efi_in[0] = 1;\n    efi_in[1] = theta;\n\n    float cos_theta = efi(efi_in, 2);\n\n    cos_theta = itf(cos_theta);\n    cos_theta = 0.000030518*cos_theta;\n    return cos_theta;\n}\n\nfloat sign(float in) {\n    return in > 0.0 ? 1.0 : -1.0;\n}\n\nfloat sat(float in, float sat_p, float sat_n){\n  float intermediate = satp(in, sat_p);\n  return satn(intermediate, sat_n);\n}\n\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n//                                         TRANSFORMATIONS                               //\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\nvoid clarke(float in[3], float out[2]){\n  \n    float sqrt3_2 = 0.86602540378;\n  \n    out[0] = 2.0/3.0*(in[0] - in[1]*0.5 - in[2]*0.5);\n    out[1] = 2.0/3.0*sqrt3_2*(in[1] - in[2]);\n}\n\nvoid clarke_inv(float in[2], float out[3]){\n  \n    float sqrt3_2 = 0.86602540378;\n\n    out[0] = in[0];\n    out[1] = - in[0]*0.5 + sqrt3_2*in[1];\n    out[2] = - in[0]*0.5 - sqrt3_2*in[1];\n}\n\nvoid park(float in[2], float out[2], float cos_t, float sin_t){\n  \n    //park\n    out[0] = in[0]*cos_t + in[1]*sin_t;\n    out[1] = -in[0]*sin_t + in[1]*cos_t;\n}\n\nvoid park_inv(float in[2], float out[2], float cos_t, float sin_t){\n  \n    //park\n    out[0] =  in[0]*cos_t - in[1]*sin_t;\n    out[1] =  in[0]*sin_t + in[1]*cos_t;\n}\n\nvoid abc_dq(float in[3], float out[2], float cos_t, float sin_t){\n    float ab[2];\n    clarke(in, ab);\n    park(ab, out, cos_t, sin_t);\n}\n\nvoid dq_abc(float in[2], float out[3], float cos_t, float sin_t){\n    float ab_inv[2];\n    park_inv(in,  ab_inv,  cos_t, sin_t);\n    clarke_inv(ab_inv, out);\n}\n\n\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n//                                         CONTROLLERS                                        //\n////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\nfloat pi(float err, float kp, float ki, float ts, float sat_p, float sat_n, float integrator ){\n  \n     float action_unsat = kp*err + integrator;\n\n    integrator = ki*ts*err + integrator;\n\n    integrator = sat(integrator,sat_p,  sat_n);\n    return sat(action_unsat, sat_p, sat_n);\n}\n\nfloat apply_dt_effect(float in, float dt_err){\n      float s  = sign(in);\n      float dt_effect = dt_err*s;\n     return in - dt_effect;\n}"
                ]
            },
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "sampling_frequency": 0
        }
    ],
    "interconnect": [],
    "emulation_time": 0.001,
    "deployment_mode": true
})");

    auto specs = fcore::emulator::emulator_specs();
    specs.set_specs(spec_json);
    fcore::emulator_manager em;
    em.set_specs(spec_json);
    auto programs = em.get_programs();


    auto ba = std::make_shared<bus_accessor>(true);
    custom_deployer d;
    d.set_accessor(ba);
    auto addr_map = custom_deployer_get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(specs, programs);

    auto ops = ba->get_operations();

    std::vector<uint64_t > reference_program = {0x6d000f,0xc,0x50001,0x20002,0x10003,0x70004,0x60005,0x30006,0x80007,0x40008,0x8000a,0x2000b,0x5000c,0x1000d,0x3e000e,0x3f000f,0xc,0xc,0x126,0x3f000000,0x1448c3,0x164863,0x1850e2,0x145982,0x166,0x3f2aaaab,0x185163,0x1418c2,0x166,0x3f13cd3a,0x1a5163,0x1429a3,0x161183,0x1c5161,0x146002,0x1611a3,0x182943,0x145981,0x5904,0x106,0x3c23d70a,0x184163,0x167182,0x1a6,0x3f4ccccd,0x1c59a3,0x1ff1c1,0x1c6,0x3c4ccccc,0x2059c3,0x7df201,0x166,0x3f800000,0x205fd0,0x226,0xbf800000,0x7c8a11,0x2059f0,0x248a11,0x9884,0x84263,0x106,0x3ebd70a4,0x264183,0x105082,0x89902,0x1021a3,0x1ff901,0x1021c3,0x7ff901,0x205ff0,0x7e8a11,0x2059f0,0x88a11,0x102883,0x141243,0x184142,0x101083,0x42a43,0x84041,0x4018e,0xa6002,0x106,0x3f5db3d7,0x142103,0x1a48a3,0xa51a1,0x146002,0x182103,0x84943,0x106082,0x84823,0x126,0x358637bd,0x144823,0x126,0x469c4000,0x184943,0x122043,0x140128,0x22595b,0x125183,0x1a2043,0x449a2,0x1a20a3,0x1401a8,0x22595b,0x125183,0x1a20a3,0xa49a2,0x1a2103,0x1401a8,0x22595b,0x125183,0x142103,0x84942,0x10004e,0x400ae,0xa008e,0x83043,0xc3903,0xe18a3,0x620c1,0x83861,0x1832, 0x21883, 0xc};

    ASSERT_EQ(ops[0].type, "p");
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {0xa2000c,0xc,0x10001,0x30002,0x20003,0x60004,0x70005,0x40006,0x20008,0x10009,0x3e0016,0x3d0017,0x3f0018,0xc,0xc,0x86,0x3b09421f,0x7ff881,0x86,0x40c90fdc,0xa27e8,0x7e00bb,0x7e00ae,0x86,0x40800000,0xa27e3,0x86,0x3e22f982,0xc20a3,0x86,0x477fff00,0xa20c3,0x20a5,0xa6,0xffff,0xc288d,0x86,0x3f000000,0xa2063,0xe2043,0x102822,0x23902,0xa6,0x3f2aaa9f,0xe08a3,0x21062,0x46,0x3f13cd30,0x60843,0x26,0x0,0x400ce,0xa1035,0x28a4,0x26,0x38000074,0xa2823,0x400ae,0x106,0x1,0x1200ce,0xa1115,0x28a4,0xa2823,0x200ae,0xa1063,0xc08e3,0x1028c1,0xa3802,0xc0863,0x610a3,0xa3061,0x66,0x3bd6b229,0xc1fa3,0xe18c3,0xc6,0x3851b717,0x1230e3,0xe6,0x43278d37,0x143923,0x1218a3,0x166,0x312bcc76,0x185923,0x123983,0x181903,0x1a3183,0x181fc3,0x1c1983,0x186,0x3fca5e35,0x1e4183,0x2059e3,0x161fc3,0x1e6163,0x1631e3,0x1e6,0x333a2f56,0x227942,0x144a21,0x126941,0x147121,0x128141,0x145921,0x121fa3,0x161923,0x126,0x34afe100,0x1a2923,0x126,0x3187ce8e,0x1c2923,0x121fc3,0x1e1923,0x1231e3,0x1e3923,0x126,0x2d90147a,0x204123,0x123a03,0xe1fa3,0x2060e3,0xe3203,0xc6961,0x1670c1,0xc7962,0x1648c2,0xc6,0x3386114e,0x123162,0xc3921,0xe6,0x36ada05f,0x1238c2,0xc6,0x3f800000,0xe3103,0xe30a3,0xa6,0x46b34100,0xe2943,0x7c30e3,0xe2923,0x7a30e3,0xa07ce,0xc07ae,0xe10c3,0x1008a3,0x123902,0xe08c3,0xc10a3,0xa38c1,0xc012e,0xe4802,0x106,0x3f5db3d7,0x142903,0x1620e3,0xe5161,0x144802,0x122903,0xa2143,0x848a2,0xa1fc3,0x106,0x3dc110a1,0x1240a1,0xbefc3,0x1018a3,0x64fa3,0xa4062,0x66,0x40c00000,0x102863,0xc};
    ASSERT_EQ(ops[1].type, "p");
    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);

    std::vector<uint64_t> control_writes_addresses = {
            0x443c21004, 0x443c21044,
            0x443c21008, 0x443c21048,
            0x443c2100c, 0x443c2104c,
            0x443c21010, 0x443c21050,
            0x443c21000,
            0x443c31004, 0x443c31044,
            0x443c31008, 0x443c31048,
            0x443c3100c, 0x443c3104c,
            0x443c31010, 0x443c31050,
            0x443c31014, 0x443c31054,
            0x443c31018, 0x443c31058,
            0x443c3101c, 0x443c3105c,
            0x443c31020, 0x443c31060,
            0x443c31024, 0x443c31064,
            0x443c31028, 0x443c31068,
            0x443c3102c, 0x443c3106c,
            0x443c31030, 0x443c31070,
            0x443c31000,
            0x443c10038, 0x443c1003c,
            0x443c20058, 0x443c2005c, 0x443c20060,
            0x443c10000, 0x443c20000
    };
    std::vector<uint32_t> control_writes_data = {
            0xa000a, 0x38,
            0xb000b, 0x38,
            0xc000c, 0x38,
            0xd000d, 0x38,
            4,
            0x40004, 0x38,
            0x3ec1004, 0x38,
            0x50005, 0x38,
            0x3ed1005, 0x38,
            0x60006, 0x38,
            0x3ee1006, 0x38,
            0x70007, 0x38,
            0x3ef1007, 0x38,
            0x80008, 0x18,
            0x3f01008, 0x18,
            0x90009, 0x6,
            0x3f11009, 0x6,
            12,
            0, 0,
            0, 0, 0,
            11, 8
    };

    for(int i = 0; i<ops.size()-2; i++){
        ASSERT_EQ(ops[i+2].type, "w");
        ASSERT_EQ(ops[i+2].address[0], control_writes_addresses[i]);
        ASSERT_EQ(ops[i+2].data[0], control_writes_data[i]);
    }
}