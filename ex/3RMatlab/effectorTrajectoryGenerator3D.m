function [ qchd, qchd_d1, qchd_d2 ] = effectorTrajectoryGenerator3D( t, parameters)
%effectorTrajectoryGenerator
%generates prime function, first and second derivative for
%end effector trajectory

    w = 0.015 * 2 * pi;
    k = 0.5;
    dx = 2.3;
    dy = 0.1;
    dz = 0.0;
    
    % circle (basic task)
    % qchd = [k*cos(w*t)+dx; k*sin(w*t)+dy; 0.1*k*cos(w*t)+dz];    
    %caclulate first and second derivative based on the qchd vector
    % qchd_d1 = [-k*w*sin(w*t); k*w*cos(w*t); -0.1*k*w*sin(w*t)];
    % qchd_d2 = [-k*w*w*cos(w*t); -k*w*w*sin(w*t); -0.1*k*w*w*cos(w*t)];

    % Smooth figure-8 (Lissajous curve): centered at (dx, dy), x in [dx-k, dx+k]
    qchd    = [ dx+k*sin(w*t);            dy+k/2*sin(2*w*t);          0.1*k*cos(w*t)+dz];
    qchd_d1 = [ k*w*cos(w*t);             k*w*cos(2*w*t);            -0.1*k*w*sin(w*t)];
    qchd_d2 = [-k*w^2*sin(w*t);          -2*k*w^2*sin(2*w*t);       -0.1*k*w^2*cos(w*t)];

    
end

