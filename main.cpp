#include "mbed.h"
#include "PS3.h"
#include "mbed_wait_api.h"
#include "QEI.h"
#include "PIDcontroller.h"

#define ADDRESS_MIGI_UE 0x14
#define ADDRESS_MIGI_SITA 0x22
#define ADDRESS_HIDARI_UE 0x40
#define ADDRESS_HIDARI_SITA 0x30

#define ADDRESS_ROLLER 0x60
#define ADDRESS_RACKandPINION 0x16

#define ADDRESS_ANGLECHANGE_VERTICAL 0x00
#define ADDRESS_ANGLECHANGE_HORIZONTAL 0x10

I2C i2c(D14,D15);
PS3 ps3 (A0,A1);
DigitalOut sig(D13);
DigitalOut Air(PC_8);

PID roller(50.0 , 15.0, 30.0 , 0.050);

QEI encoder_roller( PC_11, PD_2, NC, 2048, QEI::X2_ENCODING);
Ticker flip;

void send(char add,char data);
void send_asimawari(char d_mu, char d_ms, char d_hs, char d_hu);
void get_data(void);
char move_value(double value);
void get_rpm();
void get_angle();
int roller_PID(float target_rpm);


int Ry;            //ジョイコン　右　y軸
int Rx;            //ジョイコン　右　x軸
int Ly;            //ジョイコン　左　y軸
int Lx;            //ジョイコン　左　x軸
bool L1;            //L1
bool R1;            //R1
bool L2;            //L2
bool R2;            //R2
bool button_ue;     // ↑
bool button_sita;   // ↓
bool button_migi;   // →
bool button_hidari; // ←

bool button_maru;    // 〇
bool button_sankaku; // △
bool button_sikaku;  // ☐
bool button_batu;    // ✕

char motordata_asimawari;
char motordata_anglechange_horizontal;

int pulse;
double true_rpm;
double angle;

char roller_data;
char true_roller_data;

int main(){

    sig = 0;

    bool moved_asimawari;
    bool moved_anglechange_horizontal;

    char back = 0x60;
    char forward = 0xa0;
    char data = 0x80;

    encoder_roller.reset();
    flip.attach(&get_rpm,50ms);

    while (true) {
        Air = 0;

        //緊急停止
        if(ps3.getSELECTState()){
            sig = 1;
        }

        if(ps3.getSTARTState()){
            sig = 0;
        }

        get_data();
        //printf("m:%d L:%d R:%d Lx%d Ly%d\n",button_maru,L1,R1,Lx,Ly);
        printf("pulse:%6.0d motordata: %3.0d %5.2lf[rpm]  \n", pulse, true_roller_data, true_rpm);

        moved_asimawari = 0;
        moved_anglechange_horizontal = 0;

        //ジョイコン処理

/*
        if(Lx != 0 || Ly != 0){
            double value_ru,value_rs,value_lu,value_ls;   //右上,右下,左上,左下
            char data_ru,data_rs,data_lu,data_ls;

            if(Lx==-64) Lx = -63;
            if(Ly==64) Ly = 63;

            //右回転を正としたときのベクトル値計算
            value_ru = 0.7071*((double)Ly-(double)Lx)*-1;//逆転
            value_ls = 0.7071*((double)Ly-(double)Lx);   //正転
            value_rs = 0.7071*((double)Ly+(double)Lx)*-1;//逆転
            value_lu = 0.7071*((double)Ly+(double)Lx);   //正転

            data_ru = move_value(value_ru);
            data_rs = move_value(value_rs);
            data_lu = move_value(value_lu);
            data_ls = move_value(value_ls);
            
            send_asimawari(data_ru, data_rs, data_ls, data_lu);
            moved_asimawari = 1;
        }
*/

        if(ps3.getButtonState(PS3::ue)){
            if(ps3.getButtonState(PS3::migi)){
                //右前方向
                send(ADDRESS_HIDARI_UE,back);
                send(ADDRESS_MIGI_SITA,forward);
                
            }else if(ps3.getButtonState(PS3::hidari)){
                //左前方向
                send(ADDRESS_MIGI_UE,forward);
                send(ADDRESS_HIDARI_SITA,back);
                
            }else{
                //前進
                send(ADDRESS_MIGI_UE,forward);
                send(ADDRESS_HIDARI_UE,back);
                send(ADDRESS_HIDARI_SITA,back);
                send(ADDRESS_MIGI_SITA,forward);
            }
            
        }else if(ps3.getButtonState(PS3::sita)){
            if(ps3.getButtonState(PS3::migi)){
                //右後方向
                send(ADDRESS_MIGI_UE,back);
                send(ADDRESS_HIDARI_SITA,forward);
                
            }else if(ps3.getButtonState(PS3::hidari)){
                //左後方向
                send(ADDRESS_HIDARI_UE,forward);
                send(ADDRESS_MIGI_SITA,back);
                
            }else{
                //後進
                send(ADDRESS_MIGI_UE,back);
                send(ADDRESS_HIDARI_UE,forward);
                send(ADDRESS_HIDARI_SITA,forward);
                send(ADDRESS_MIGI_SITA,back);
            }

        }else if(ps3.getButtonState(PS3::hidari)){
            //左方向
            send(ADDRESS_MIGI_UE,forward);
            send(ADDRESS_HIDARI_UE,forward);
            send(ADDRESS_HIDARI_SITA,back);
            send(ADDRESS_MIGI_SITA,back);
            
        }else if(ps3.getButtonState(PS3::migi)){
            //右方向
            send(ADDRESS_MIGI_UE,back);
            send(ADDRESS_HIDARI_UE,back);
            send(ADDRESS_HIDARI_SITA,forward);
            send(ADDRESS_MIGI_SITA,forward);
            
        }else{
            //停止
            send(ADDRESS_MIGI_UE,0x80);
            send(ADDRESS_HIDARI_UE,0x80);
            send(ADDRESS_HIDARI_SITA,0x80);
            send(ADDRESS_MIGI_SITA,0x80);
        }

        //PID制御で射出機構のモーターの回転数維持
        if(ps3.getButtonState(PS3::maru)){
            roller_PID(2500.0);
            send(ADDRESS_ROLLER, true_roller_data);
            //send(0x30, true_roller_data);
            //send(0x24, true_roller_data);
        }else{
            send(ADDRESS_ROLLER, 0x80);
            //send(0x30, 0x80);
            //send(0x24, 0x80);
        }


        //機体左に旋回
        if(L1 && !moved_asimawari){
            motordata_asimawari = forward;
            send_asimawari(motordata_asimawari,motordata_asimawari,motordata_asimawari,motordata_asimawari);
            moved_asimawari = 1;
        }

        //機体右に旋回
        if(R1 && !moved_asimawari){
            motordata_asimawari = back;
            send_asimawari(motordata_asimawari,motordata_asimawari,motordata_asimawari,motordata_asimawari);
            moved_asimawari = 1;
        }
        
        //砲台を左に旋回
        if(L2 && !moved_anglechange_horizontal){
            motordata_anglechange_horizontal = forward;
            send(ADDRESS_ANGLECHANGE_HORIZONTAL, motordata_anglechange_horizontal);
            moved_anglechange_horizontal = 1;
        }
        //砲台を右に旋回
        if(R2 && !moved_anglechange_horizontal){
            motordata_anglechange_horizontal = back;
            send(ADDRESS_ANGLECHANGE_HORIZONTAL, motordata_anglechange_horizontal);
            moved_anglechange_horizontal = 1;
        }

/*        if(button_maru){
            if(rpm>2500){
                send(ADDRESS_ROLLER, 0x5c);
            }else if(rpm<1000){
                send(ADDRESS_ROLLER, 0x5e);
            }else if(rpm<=2000){
                send(ADDRESS_ROLLER, 0x30);
            }else if(rpm<=2500){
                send(ADDRESS_ROLLER, 0x40);
            }
        }else{
            send(ADDRESS_ROLLER, 0x80);
        }
*/
        //エアシリンダー
        if(button_sankaku){
            Air = 1;
        }else{
            Air = 0;
        }

        //装填機構
        if(ps3.getButtonState(PS3::sikaku)){
            send(ADDRESS_RACKandPINION, 0x95);  //正転
            
        }
        if(ps3.getButtonState(PS3::batu)){
            send(ADDRESS_RACKandPINION, 0x65);  //逆転
        }

/*        //足回り静止
        if(!moved_asimawari){
            motordata_asimawari = 0x80;  
            send_asimawari(motordata_asimawari,motordata_asimawari,motordata_asimawari,motordata_asimawari);
        }
*/
        //砲台停止
        if(!moved_anglechange_horizontal){
            motordata_anglechange_horizontal = 0x80;
            send(ADDRESS_ANGLECHANGE_HORIZONTAL, motordata_anglechange_horizontal);
        }

        
        
    }
}

//データ取得のための関数
void get_data(void){
    //ボタン取得
    button_ue = ps3.getButtonState(PS3::ue);
    button_sita= ps3.getButtonState(PS3::sita);
    button_migi = ps3.getButtonState(PS3::migi);
    button_hidari = ps3.getButtonState(PS3::hidari);

    L1 = ps3.getButtonState(PS3::L1);
    R1 = ps3.getButtonState(PS3::R1);
    L2 = ps3.getButtonState(PS3::L2);
    R2 = ps3.getButtonState(PS3::R2);

    button_maru = ps3.getButtonState(PS3::maru);
    button_sankaku = ps3.getButtonState(PS3::sankaku);

    //スティックの座標取得
    Ry = ps3.getRightJoystickYaxis();
    Rx = ps3.getRightJoystickXaxis();
    Ly = ps3.getLeftJoystickYaxis();
    Lx = ps3.getLeftJoystickXaxis();
}


void send(char address, char data){
    wait_us(15000);
    i2c.start();
    i2c.write(address);
    i2c.write(data);
    i2c.stop();
}

void send_asimawari(char d_mu, char d_ms, char d_hs, char d_hu){
    send(ADDRESS_MIGI_UE,d_mu);
    send(ADDRESS_MIGI_SITA,d_ms);
    send(ADDRESS_HIDARI_SITA,d_hs);
    send(ADDRESS_HIDARI_UE,d_hu);
}
//取得座標から回転速度を求める関数
char move_value(double value){
    double uv = value;
    int rate, move;
    char result;

    //絶対値化
    if(value<0) uv = uv*-1;

    //割合計算(小数点以下四捨五入=>整数)
    //座標の絶対値の最高値が63と64になるので小さいほうの63に合わせて割合を出す
    rate = (double)uv/89.0954*100+0.5;
    
    //64だと100%を超えるのでその調整
    if(rate>100) rate = 100;

    //割合をもとに出力するパワーを計算(整数)
    rate = (double)rate/100.0*123.0;
    
    //正転と逆転の処理
    //引数がマイナスの時は逆転、プラスの時は正転、0の時には静止
    if(!value) move = 128;
    else if(value<0) move = 124-rate;
    else move = rate + 132;

    //型変更
    result = (char)move;
    return result;
}

void get_rpm(){
    pulse = encoder_roller.getPulses();
    encoder_roller.reset();
    true_rpm = (60*20*(double)pulse) / (2048*2) ;
}

void get_angle(){
    pulse = encoder_roller.getPulses();
    angle = (360*(double)pulse)/(2048*2);
}

int roller_PID(float target_rpm){

    //入力値（rpm）の範囲定義
    roller.setInputLimits(-5000.0,5000.0);

    if(true_rpm <= target_rpm){
        roller.setOutputLimits(0x84, 0xff);
    }else if(true_rpm > target_rpm){
        roller.setOutputLimits(0x00, 0x7B);
    }

    roller.setSetPoint(target_rpm);//目標値設定

    roller.setProcessValue(true_rpm);//現在の値（rpm）

    roller_data = roller.compute();

    if(true_rpm <= target_rpm){
        true_roller_data = roller_data;
    }else if(true_rpm > target_rpm){
        true_roller_data = 0x7b - roller_data;
    }

    return 0;
}