#include "pvt.h"
#include "GPSEphemerisStore.hpp"



double distance(Vector3d a, Vector3d b)
{
    return (a - b).norm();
}

void genetate_data(vector<pvt_obs_t>& pvt_obs, VectorXd& x)
{
    x = VectorXd::Random(4) * 100000;
    cout << "xx: " << x.transpose() << endl;

    for (int i = 0; i < pvt_obs.size(); i++)
    {
        pvt_obs[i].sat_pos = VectorXd::Random(3) * 1000000;
        pvt_obs[i].P = distance(pvt_obs[i].sat_pos, x.segment(0, 3)) + x(3);
        cout << "sat: " << i << "  P:" << pvt_obs[i].P << "  " << pvt_obs[i].sat_pos.transpose() << endl;
    }

}

double sat_elevation_angle(Vector3d sat_pos, Vector3d rcv_pos)
{
    if (sat_pos.norm() < 1e-6 || rcv_pos.norm() < 1e-6)
    {
        return M_PI / 2;
    }
    double cc = sat_pos.dot(rcv_pos) / (sat_pos.norm() * rcv_pos.norm());
    cout << "cc: " << cc << " acos(cc): " << acos(cc) << endl;
    return acos(cc);
}

void residual(VectorXd x, vector<pvt_obs_t> pvt_obs, VectorXd& res, MatrixXd& H)
{
    int n = pvt_obs.size();
    res = VectorXd::Zero(n);
    H = MatrixXd::Zero(n, 4);
    for (int i = 0; i < n; i++)
    {
        pvt_obs_t obs = pvt_obs[i];
        Vector3d sat_pos = obs.sat_pos;
        double P = obs.P;
        double rho = distance(sat_pos, x.segment(0, 3));
        res(i) = P - rho - x(3);
        cout << "P: " << P << "  rho : " << rho << "  y: " << res(i) << endl;
        H(i, 0) = (x(0) - sat_pos(0)) / rho;
        H(i, 1) = (x(1) - sat_pos(1)) / rho;
        H(i, 2) = (x(2) - sat_pos(2)) / rho;
        H(i, 3) = 1.0;
    }
}

//TODO
void ecef2geodetic(Vector3d xyz, Vector3d& llh)
{
    double p = xyz.segment(0, 2).norm();
    llh[0] = atan2(xyz(2), p);//纬度
    llh[1] = xyz.norm(); //高度
    llh[2] = p / cos(llh(0));//经度
}
/// defined in ICD-GPS-200C, 20.3.3.4.3.3 and Table 20-IV
/// @return angular velocity of Earth in radians/sec.
double angVelocity() 
{
    return 7.2921151467e-5;
}
//solve receiver position
void pvt_solver(vector<pvt_obs_t> pvt_obs)
{
    VectorXd x = Vector4d::Zero(4);
    const double C_MPS = 2.99792458e8;
    //VectorXd x = Vector4d(-3978242.4348, 3382841.1715, 3649902.7667, 0);
    //genetate_data(pvt_obs, x);
    //cout << "xx2: " << x.transpose() << endl;
    x = Vector4d::Zero(4);
    int n = pvt_obs.size();

    double err = 1e8;
    int cnt = 0;
    VectorXd y = VectorXd::Zero(n);
    MatrixXd H = MatrixXd::Zero(n, 4);
    MatrixXd W = MatrixXd::Zero(n, n);
    // for (int i = 0; i < n; i++)
    // {
    //     cout << "sat: " << i << "  P:" << pvt_obs[i].P  << "  " << pvt_obs[i].sat_pos.transpose() << endl;
    // }
    // for (int i = 0; i < n; i++)
    // {
    //     pvt_obs_t obs = pvt_obs[i];
    //     Vector3d sat_pos = obs.sat_pos;
    //     double P = obs.P;
    //     double rho = distance(sat_pos, x.segment(0, 3));
    //     y(i) = P - rho - x(3);
    //     //cout << "P: " << P << "  rho : " << rho << "  y: " << y(i) << endl;
    // }
    while (err > 0.01 && cnt < 20)
    {
        cout << "cnt : <<<<<<<<<<<<<<<<<<<<<<<<<<" << cnt << endl;
        for (int i = 0; i < n; i++)
        {
            pvt_obs_t obs = pvt_obs[i];
            //TODO update sat pos
            Vector3d sat_pos = obs.sat_pos;
            double P = obs.P;
            double rho = distance(sat_pos, x.segment(0, 3));
            double tx = rho / C_MPS;
            y(i) = P - rho - x(3);

            // correct for earth rotation
            double wt = angVelocity() * tx; // radians
            sat_pos(0) = cos(wt) * sat_pos(0) + sin(wt) * sat_pos(1);
            sat_pos(1) = -sin(wt) * sat_pos(0) + cos(wt) * sat_pos(1);
            sat_pos(2) = sat_pos(2);


            //trop correction
            Vector3d llh;
            ecef2geodetic(x.segment(0, 3), llh);
            rho = distance(sat_pos, x.segment(0, 3));

            double elev_angle = sat_elevation_angle(sat_pos, x.segment(0, 3));
            W(i, i) = 1 / (sin(elev_angle) * sin(elev_angle) + 1e-6);

            

            Vector3d I = x.segment(0, 3) - sat_pos;
            I.normalize();
            H.block(i, 0, 1, 3) = I.transpose();
            H(i, 3) = 1.0;



            cout << "P: " << P << "  rho : " << rho  << "  y: " << y(i)  << "  weight: " << W(i, i) << " elev angle: " << elev_angle << endl;
        }
        MatrixXd HH = H.transpose() * W * H;
        VectorXd dx = HH.ldlt().solve(H.transpose()* W * y);
        //cout << "H: " << H << endl;
        cout << "y: " << y.transpose() << endl;
        cout << "dx: " << dx.transpose() << endl;
        cout << "H * dx: " << (H*dx).transpose() << endl;
        x += dx;
        err = dx.norm();
        cout << "x: " << x.transpose() << endl;
        cout << "err: " << err << endl;
        cnt++;
        cout << "===============================" << endl;
    }

}

