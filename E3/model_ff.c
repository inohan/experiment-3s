//----------------------------------------------------------------------------------------
// title
// 　簡易包絡線モデルに基づいた電流包絡線PID制御
// designer
// 　ryo matsumoto
//----------------------------------------------------------------------------------------

#include <mwio4.h>

#define BDN0 0         // PEVボード
#define BDN1 0         // FPGAボード
                       // WN WP VN VP UN UP
#define TRANS_MODE 80  // 00 00 01 01 00 00
#define INV_FREQ 85000
#define DEAD_TIME 300

INT32 adc_0_data_peak;
float i1_ampl;
volatile float i1_weight_pos = 1.1;
volatile float i1_weight_neg = 1;
volatile float i1_ampl_ref;
float v1_ampl_ref = 0;
volatile int set_pwm_on_trans_w_o_control = 0;
volatile int set_pwm_on_trans_w_control = 0;
int control_on = 0;
volatile float proportionalGain = 1.509;
volatile float integralGain = 2.659e+5;
volatile float derivativeGain = 7.898e-4;
volatile float tau = 3.719e-5;
float r_0, r_1, r_2, c_0, c_1, c_2 = 0;
float period = 1 / (float)INV_FREQ / 2;
volatile float v1dc;
float inv_mod_BDN0 = 1;
volatile float duty_w_o_control = 0;
const float pi = 3.14;

//----------------------------------------------------------------------------------------
// 　arccos関数
//----------------------------------------------------------------------------------------

float arccos(float x) {
    float y = mwarctan(mwsqrt(1 - x * x) / x);
    return y;
}

float gainfunc(float T, float r0, float r1, float r2, float c1, float c2) {
    float alpha1 = 2.689e04;
    float alpha0 = 1.161e09;
    float beta1 = 2033;
    float beta0 = 5.465e07;
    float a2 = (period * beta0 - 2 * beta1) * (period - 2 * T);
    float a1 = (2 * beta1 + period * beta0) * (period - 2 * T) + (period * beta0 - 2 * beta1) * (period + 2 * T);
    float a0 = (2 * beta1 + period * beta0) * (period + 2 * T);
    float b2 = (4 - 2 * period * alpha1 + period * period * alpha0);
    float b1 = (-8 + 2 * period * period * alpha0);
    float b0 = (4 + 2 * period * alpha1 + period * period * alpha0);
    return (b2 * r2 + b1 * r1 + b0 * r0 - a2 * c2 - a1 * c1) / a0;
}

//----------------------------------------------------------------------------------------
// 　電流包絡線検波
//----------------------------------------------------------------------------------------

void read_envelope(void) {
    adc_0_data_peak = IPFPGA_read(BDN1, 0x17);
    //    i1_ampl = mwabs(adc_0_data_peak * 125. / 8000 * i1_weight);
    if (adc_0_data_peak < 0) {
        i1_ampl = mwabs(adc_0_data_peak * 125. / 8000 * i1_weight_neg);
    } else {
        i1_ampl = mwabs(adc_0_data_peak * 125. / 8000 * i1_weight_pos);
    }
}

//----------------------------------------------------------------------------------------
// 　電流制御
//----------------------------------------------------------------------------------------

void current_control(void) {
    if (control_on == 1) {
        r_2 = r_1;
        r_1 = r_0;
        r_0 = i1_ampl_ref;
        c_2 = c_1;
        c_1 = c_0;
        // FF Control
        c_0 = gainfunc(5e-4, r_0, r_1, r_2, c_1, c_2);
        v1_ampl_ref = c_0;
        // 電流指令値 i1_ampl_ref と電流包絡線の測定値 i1_ampl から
        // 出力電圧の指令値 v1_ampl_ref を求めるコードを書く
        if (v1_ampl_ref < 0) {
            v1_ampl_ref = 0;
        } else if (v1_ampl_ref > v1dc * 4 / pi) {
            v1_ampl_ref = v1dc * 4 / pi;
        }

        inv_mod_BDN0 = 2 / pi * arccos(pi * v1_ampl_ref / (4 * v1dc));

        if (inv_mod_BDN0 < 0) {
            inv_mod_BDN0 = 0;
        } else if (inv_mod_BDN0 > 1) {
            inv_mod_BDN0 = 1;
        }

        PEV_inverter_set_uvw(BDN0, -inv_mod_BDN0, inv_mod_BDN0, 0, 0);
    } else {
        PEV_inverter_set_uvw(BDN0, duty_w_o_control - 1, 1 - duty_w_o_control, 0, 0);
    }
}

//----------------------------------------------------------------------------------------
// 　割り込み関数
//----------------------------------------------------------------------------------------

interrupt void sync_interrupt(void) {
    int0_ack();
    read_envelope();
    current_control();
}

//----------------------------------------------------------------------------------------
// 　初期化
//----------------------------------------------------------------------------------------

void initialize(void) {
    PEV_init(BDN0);

    int_disable();

    PEV_inverter_disable_int(BDN0);

    PEV_inverter_init(BDN0, INV_FREQ, DEAD_TIME);
    PEV_inverter_init_int_timing(BDN0, 3, 0, 0);
    PEV_int_init(BDN0, 2, 0, 0, 0, 0, 0, 0, 0);
    int0_init_vector(sync_interrupt, (CSL_IntcVectId)7, FALSE);
    PEV_inverter_control_gate(BDN0, TRANS_MODE);
    PEV_inverter_set_uvw(BDN0, 0, 0, 0, 0);

    PEV_inverter_enable_int(BDN0);

    int0_enable_int();

    int_enable();
}

//----------------------------------------------------------------------------------------
// 　メイン
//----------------------------------------------------------------------------------------

void MW_main(void) {
    initialize();

    while (1) {
        if (set_pwm_on_trans_w_control == 1) {
            control_on = 1;
            PEV_inverter_start_pwm(BDN0);
            set_pwm_on_trans_w_control = -1;
        }
        if (set_pwm_on_trans_w_o_control == 1) {
            PEV_inverter_start_pwm(BDN0);
            set_pwm_on_trans_w_o_control = -1;
        }
    }
}