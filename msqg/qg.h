/**
   multiple scale quasi-geostrophic model 
*/

#include "predictor-corrector.h"
#include "poisson.h"
#include "timestep.h"

// layered variables
scalar * qol  = NULL; // vorticity on layers
scalar * pol  = NULL; // stream function on layers
scalar * qom  = NULL; // vorticity on vertical modes
scalar * pom  = NULL; // stream function on modes
scalar * zetal = NULL; // relative vorticity
scalar * pofl = NULL; // filter mean
scalar * qosl = NULL; // filter mean


// large scale variables (inversion matrices, etc)
scalar * ppl  = NULL; // large scale stream function
scalar * cl2m  = NULL;
scalar * cm2l  = NULL;
scalar * iBul = NULL; // inverse burger number
scalar * gpl = NULL;
scalar sig_filt[];
scalar sig_lev[];
int lsmin, lsmax; 

// temporary psi field with samem BC as p
scalar * tmpl = NULL;

scalar * evolving = NULL;

mgstats mgpsi;

scalar Ro[];

int nl = 1;
double * hl;

double Lt = 100; // size of the domain
double Re = 1e1; // reynolds number
double Re4 = 1e3; // bihormonic reynolds number
double Ek = 1e-2; // Ekman number
double Rom = 1e-2; // Mean Rossby number
double beta = 0.5;
double tau0 = 0.; // wind stress curl
double sbc = 0.; // slip BC: 0: free slip (OK), big: no slip (TO BE FINISHED)

double tend = 1; // end time
double dtfilter = 1; // Delat T filtering 
double dtout = 1; // Delat T output 

char * outdir = "./";

int nbar = 0;

#include "eigmode.h"

trace
void invertq(scalar * pol, scalar * qol)
{
  // convert to modes: matrix multiplication cl2m*qo
  foreach(){
    // outer loop on modes
    for (int m = 0; m < nl ; m++) {
      scalar qm  = qom[m];
      qm[] = 0.;
      
      // inner loop on layers
      for (int l = 0; l < nl ; l++) {
        scalar qo  = qol[l];
        scalar l2m = cl2m[m*nl+l];
        qm[] += l2m[]*qo[];
        }      
      /* No rescaling by 1/Ro because we multiply the equation by
         1/Ro */
      /* qm[] = qm[]/Ro[]; */
      }
    }

  boundary(qom);
//  boundary(pom);
  // elliptic solver on modal pressures
  for (int l = 0; l < nl ; l++) {
    scalar qm  = qom[l];
    scalar pm  = pom[l];
    scalar iBu  = iBul[l];
    mgpsi = poisson (pm, qm, lambda = iBu, tolerance = 1e-3);
  }

  // back to layers : matrix multiplication cm2l*pm
  foreach(){
    // outer loop on layers
    for (int l = 0; l < nl ; l++) {
      scalar po  = pol[l];
      po[] = 0.;
      
      // inner loop on modes
      for (int m = 0; m < nl ; m++) {
        scalar pm  = pom[m];
        scalar m2l = cm2l[l*nl+m];
        po[] += m2l[]*pm[];
      }
    }
  }

  boundary(pol);

}

trace
void comp_del2(scalar * pol, scalar * zetal, double add, double fac)
{
  foreach()
    for (int l = 0; l < nl ; l++) {
      scalar po = pol[l];
      scalar zeta = zetal[l];
      zeta[] = add*zeta[] 
        + fac*(po[1] + po[-1] + po[0,1] + po[0,-1] - 4*po[])/(sq(Delta));
    }

  boundary(zetal);
  
  /**
   Slip BC*/
  if (sbc > 0) {
    for (int l = 0; l < nl ; l++) {
      scalar po = pol[l];
      scalar zeta = zetal[l];
      foreach_boundary(left) {
        zeta[ghost] = add*zeta[ghost] 
          + fac*sbc/((0.5*sbc + 1)*sq(Delta))*(po[]-po[ghost]);}
      foreach_boundary(right) {
        zeta[ghost] = add*zeta[ghost] 
          + fac*sbc/((0.5*sbc + 1)*sq(Delta))*(po[]-po[ghost]);}
      foreach_boundary(top) {
        zeta[ghost] = add*zeta[ghost] 
          + fac*sbc/((0.5*sbc + 1)*sq(Delta))*(po[]-po[ghost]);}
      foreach_boundary(bottom) {
        zeta[ghost] = add*zeta[ghost] 
          + fac*sbc/((0.5*sbc + 1)*sq(Delta))*(po[]-po[ghost]);}
    }
  }

}

trace
void comp_strech(scalar * pol, scalar * strechl, double add)
{

  foreach() {

    if (nl > 1){
      // upper layer
      int l = 0;
      scalar po_1 = pol[l];
      scalar po_2 = pol[l+1];
      scalar strech = strechl[l];
      scalar gp1 = gpl[l];
      double a2 = 1./( gp1[]*hl[l]*sq(Ro[]));

      strech[] = add*strech[] + a2*po_2[] - a2*po_1[] ;

      // intermediate layers
      for (int l = 1; l < nl-1 ; l++) {
       
        scalar po_0 = pol[l-1];
        scalar po_1 = pol[l];
        scalar po_2 = pol[l+1];
        scalar strech = strechl[l];
        
        scalar gp0 = gpl[l-1];
        scalar gp1 = gpl[l];
        
        double a0 = 1./( gp0[]*hl[l]*sq(Ro[]));
        double a2 = 1./( gp1[]*hl[l]*sq(Ro[]));

        strech[] = add*strech[] + a0*po_0[] + a2*po_2[] - (a0 + a2)*po_1[] ;

      }
      // lower layer
      l = nl-1;
      po_1 = pol[l-1];
      po_2 = pol[l];
      strech = strechl[l];
      gp1 = gpl[l-1];
      a2 = 1./( gp1[]*hl[l]*sq(Ro[]));
      strech[] = add*strech[] + a2*po_1[] - a2*po_2[] ;

    }
    else{
      scalar strech = strechl[0];
      strech[] = 0.;
    }
  }

  boundary(strechl);
}


trace
void combine_jac(scalar * jac1l, scalar * jacal, double add)
{
  foreach() {
    if (nl > 1){
      // upper layer
      int l = 0;
      scalar jac1 = jac1l[l];
      scalar jaca = jacal[l];
      scalar gp1 = gpl[l];
      double a2 = 1./( gp1[]*hl[l]*sq(Ro[]));

      jaca[] = add*jaca[] + a2*jac1[];

      // intermediate layers
      for (int l = 1; l < nl-1 ; l++) {
       
        scalar jac0 = jac1l[l-1];
        scalar jac1 = jac1l[l];
        scalar jaca = jacal[l];
        
        scalar gp0 = gpl[l-1];
        scalar gp1 = gpl[l];
        
        double a0 = 1./( gp0[]*hl[l]*sq(Ro[]));
        double a2 = 1./( gp1[]*hl[l]*sq(Ro[]));

        jaca[] = add*jaca[] - a0*jac0[] + a2*jac1[];

      }
      // lower layer
      l = nl-1;
      jac1 = jac1l[l-1];
      jaca = jacal[l];
      gp1 = gpl[l-1];
      a2 = 1./( gp1[]*hl[l]*sq(Ro[]));
      jaca[] = add*jaca[] - a2*jac1[];

    }
    else{
      scalar jaca = jacal[0];
      jaca[] = 0.;
    }
  }
}

trace
void comp_q(scalar * pol, scalar * qol)
{
  comp_del2  (pol, qol, 0., 1.);
  comp_strech(pol, qol, 1.);
  // TODO: not the rigght BC if partial or no slip
  boundary(qol);
}

trace
void comp_vel(const scalar po, face vector uf)
{

  struct { double x, y; } f = {-1.,1.};
  foreach_face() {
    uf.x[] = f.x*Ro[]*(po[] - po[0,-1])/(Delta);
  }
}


/**
   Arakawa jacobian: J(q,p)
*/
trace
void jacobian(scalar po, scalar qo, scalar jac, double add)
{
  foreach()
    jac[] = add*jac[] + 
    (( qo[1, 0 ]-qo[-1, 0])*(po[0, 1]-po[ 0 ,-1])
     +(qo[0 ,-1]-qo[ 0 ,1])*(po[1, 0]-po[-1, 0 ])
     + qo[1, 0 ]*( po[1,1 ] - po[1,-1 ])
     - qo[-1, 0]*( po[-1,1] - po[-1,-1])
     - qo[ 0 ,1]*( po[1,1 ] - po[-1,1 ])
     + qo[0 ,-1]*( po[1,-1] - po[-1,-1])
     + po[ 0 ,1]*( qo[1,1 ] - qo[-1,1 ])
     - po[0 ,-1]*( qo[1,-1] - qo[-1,-1])
     - po[1, 0 ]*( qo[1,1 ] - qo[1,-1 ])
     + po[-1, 0]*( qo[-1,1] - qo[-1,-1]))
    *Ro[]/(12.*Delta*Delta);
}

/**
   beta effect times f
 */
void beta_effect(scalar po, scalar jac)
{
    foreach()
      jac[] -= beta*(po[1] - po[-1])/(2*Delta);
}

trace
double advection  (scalar * qol, scalar * pol, scalar * dqol, double dtmax)
{  
  for (int l = 0; l < nl ; l++) {
    scalar qo  = qol[l];
    scalar po  = pol[l];
    scalar pp  = ppl[l];
    scalar dqo = dqol[l];
    jacobian(po, qo, dqo, 1.); // J(zeta, p_qg)
    jacobian(pp, qo, dqo, 1.); // J(zeta, p_pg)
    beta_effect(po, dqo);
  }
  
   for (int l = 0; l < nl-1 ; l++) {
    scalar po1  = pol[l];
    scalar pp1  = ppl[l];
    scalar po2  = pol[l+1];
    scalar jac1 = tmpl[l];
    scalar pp2  = ppl[l+1];
    jacobian(po1, po2, jac1, 0.);
    jacobian(pp1, po2, jac1, 1.);
    jacobian(po1, pp2, jac1, 1.);
  }
 combine_jac(tmpl, dqol, 1.);

  // compute dtmax
   for (int l = 0; l < nl ; l++) {
     face vector uf[];
     scalar po = pol[l]; 
     comp_vel(po, uf);
     dtmax = timestep (uf, dtmax);
     po = ppl[l]; 
     comp_vel(po, uf);
     dtmax = timestep (uf, dtmax);
  //    dtmax = DT;
}

  return dtmax;
}


trace
void dissip  (scalar * zetal, scalar * dqol)
{
  // no multiplication by Ro because we multiply the entire
  // equation by Ro
  double iRe = 1/Re;
  comp_del2(zetal, tmpl, 0., 1.);

  foreach() 
    for (int l = 0; l < nl ; l++) {
      scalar dqo = dqol[l];
      scalar p4 = tmpl[l];
      dqo[] += p4[]/Re;
    }

  double iRe4 = -1/Re4;
  comp_del2(tmpl, dqol, 1., iRe4);

}

/**
   Ekman Friction on bottom layer
*/

trace
void bottom_friction  (scalar * zetal, scalar * dqol)
{
  scalar dqo = dqol[nl-1];
  scalar zeta = zetal[nl-1];
  foreach()
    dqo[] -= Ek*zeta[];
}

/**
   surface forcing
*/

trace
void surface_forcing  (scalar * dqol)
{
  scalar dqo = dqol[0];
  foreach() 
    dqo[] -= tau0*sin(2*pi*y/Lt);
}


trace
void time_filter (scalar * qol, scalar * qo_mel, double dt)
{

  double tau_f = 20;
  double alpha_f = dt/tau_f;


  scalar qo_me[];
  scalar qo[];
  foreach(){
    for (qo_me, qo in qo_mel, qol){
      qo_me[] = (1-alpha_f)*qo_me[] + alpha_f*qo[];
//      qo[] = qo[] - qo_me[];
    }
  }
}

trace
void wavelet_filter(scalar *qol, scalar * qofl, double dtfilter)
{
  /* save q values */
//  scalar * qosl = list_clone(qol);
  foreach()
    for (int l = 0; l < nl ; l++) {
      scalar qo  = qol[l];
      scalar qos  = qosl[l];
      qos[] = qo[];
    }
  
  for (scalar qo in qol) {
    scalar w[];
    w[top] = 0;
    w[bottom] = 0;
    w[right] = 0;
    w[left] = 0;


    wavelet(qo,w);
    for (int l = 0; l <= depth(); l++) {
      foreach_level (l)
        w[] *= sig_lev[];
      boundary_level ({w}, l);
    }
    inverse_wavelet (qo, w);

  }
  
  foreach()
    for (int l = 0; l < nl ; l++) {
      scalar qo  = qol[l];
      scalar qof  = qofl[l];
      scalar qos  = qosl[l];

      qof[] = (qof[]*nbar + (qos[] - qo[])/dtfilter)/(nbar+1);
      //qof[] = (qos[] - qo[]);
    }
  boundary(qofl);
  nbar++;
  
}

/**
   
   ## time stepping routines
   We use the predictor corrector implementation */

static void advance_qg (scalar * output, scalar * input,
                        scalar * updates, double dt)
{
  foreach() {
    for (int l = 0; l < nl ; l++) {
      scalar qi = input[l];
      scalar qo = output[l];
      scalar dq = updates[l];
      qo[] = qi[] + dq[]*dt;
    }
  }
  boundary(output);
}




double update_qg (scalar * evolving, scalar * updates, double dtmax)
{
  foreach()
    for (scalar s in updates)
      s[] = 0.;

  invertq(pol, evolving);
  comp_del2(pol, zetal, 0., 1.0);
  dtmax = advection(zetal, pol, updates, dtmax);
  dissip(zetal,updates);
  bottom_friction(zetal,updates);
  surface_forcing(updates);

  return dtmax;
}

/**
   
   ## Layerered variables initialization, etc
*/

void set_vars()
{
  assert (qol    == NULL);
  assert (pol    == NULL);
  assert (qom    == NULL);
  assert (pom    == NULL);
  assert (ppl    == NULL);
  assert (cl2m   == NULL);
  assert (cm2l   == NULL);
  assert (iBul   == NULL);
  assert (gpl    == NULL);
  assert (zetal  == NULL);
  assert (tmpl   == NULL);

  assert (nl > 0);

  for (int l = 0; l < nl; l++) {
    scalar po = new scalar;
    pol = list_append (pol, po);
    scalar qo = new scalar;
    qol = list_append (qol, qo);
    scalar pm = new scalar;
    pom = list_append (pom, pm);
    scalar qm = new scalar;
    qom = list_append (qom, qm);
    scalar zeta = new scalar;
    zetal = list_append (zetal, zeta);
    scalar pof = new scalar;
    pofl = list_append (pofl, pof);
    scalar qos = new scalar;
    qosl = list_append (qosl, qos);

    scalar pp = new scalar;
    ppl = list_append (ppl, pp);
    scalar iBu = new scalar;
    iBul = list_append (iBul, iBu);
    scalar gp = new scalar;
    gpl = list_append (gpl, gp);
    
    scalar pt = new scalar;
    tmpl = list_append (tmpl, pt);

    po[right]  = dirichlet(0);
    po[left]   = dirichlet(0);
    po[top]    = dirichlet(0);
    po[bottom] = dirichlet(0);
    
    qo[right]  = dirichlet(0);
    qo[left]   = dirichlet(0);
    qo[top]    = dirichlet(0);
    qo[bottom] = dirichlet(0);

    pm[right]  = dirichlet(0);
    pm[left]   = dirichlet(0);
    pm[top]    = dirichlet(0);
    pm[bottom] = dirichlet(0);
    
    qm[right]  = dirichlet(0);
    qm[left]   = dirichlet(0);
    qm[top]    = dirichlet(0);
    qm[bottom] = dirichlet(0);

    zeta[right]  = dirichlet(0);
    zeta[left]   = dirichlet(0);
    zeta[top]    = dirichlet(0);
    zeta[bottom] = dirichlet(0);

    pof[right]  = dirichlet(0);
    pof[left]   = dirichlet(0);
    pof[top]    = dirichlet(0);
    pof[bottom] = dirichlet(0);

    pp[right]  = dirichlet(0);
    pp[left]   = dirichlet(0);
    pp[top]    = dirichlet(0);
    pp[bottom] = dirichlet(0);

    pt[right]  = dirichlet(0);
    pt[left]   = dirichlet(0);
    pt[top]    = dirichlet(0);
    pt[bottom] = dirichlet(0);

  }
  evolving = qol;
  
  /**
     Mode to layer inversion matrices (dimesnion: $nl^2$)
  */
  
  for (int l = 0; l < nl*nl; l++) {
    scalar l2 = new scalar;
    cl2m = list_append (cl2m, l2);
    scalar m2 = new scalar;
    cm2l = list_append (cm2l, m2);
  }
  
  /**
     Default variables:
     Layer thicknesses hl
     Reduced gravity gpl
     Rossby number Ro
  */
  hl = malloc (nl*sizeof(double));
  for (int l = 0; l < nl; l++)
    hl[l] = 0.1/nl;

  foreach() 
    for (scalar gp in gpl)
      gp[] =  1e6; //2e-2*lref/sq(uref);

  foreach()
    Ro[] = Rom; // u=0.1m/s,  f=1e-4s-1,  l=50km

  /**
     Initialize variables */
  foreach() {
    for (scalar po in pol){po[] = 0.0;} 
    for (scalar qo in qol){qo[] = 0.0;}
    for (scalar pm in pom){pm[] = 0.0;}
    for (scalar qm in qom){qm[] = 0.0;}
    for (scalar zeta in zetal){zeta[] = 0.0;}
    for (scalar pof in pofl){pof[] = 0.0;}
    for (scalar pt in tmpl){pt[] = 0.0;}
    for (scalar qos in qosl){qos[] = 0.0;}
  }
  /**
     We overload the default 'advance' and 'update' functions of the
     predictor-corrector scheme and (TODO) setup the prolongation and
     restriction methods on trees. */

  advance = advance_qg;
  update = update_qg;

}


event defaults (i = 0){
  set_vars();
}


/**
The event below will happen after all the other initial events to take
into account user-defined field initialisations. */

event init (i = 0)
{

  /* foreach() */
  /*   Ro[] = uref/((fref + betad*(y-0.5*L0)*lref)*lref); */
  foreach()
    Ro[] = Rom/(1 + Rom*beta*(y-0.5*L0));

  /**
     compute PV inversion matrices  */
  eigmod(hl, Ro, gpl, cl2m, cm2l, iBul);

  /**
     compute filter length scale and wavelet coeffs*/
  scalar iRd = iBul[1];
  foreach()
    sig_filt[] = min(15*sqrt(-1/iRd[]),Lt/8);


  restriction({sig_filt});

  // low pass filter
  for (int l = depth(); l >= 0; l--) {
    foreach_level (l) {
      double ref_flag = 0;
      if (l < depth())
        foreach_child()
          ref_flag += sig_lev[];
      if (ref_flag > 0)
        sig_lev[] = 1;
      else
        if (sig_filt[] > 2*Delta)
          sig_lev[] = 0;
        else if (sig_filt[] <= 2*Delta && sig_filt[] > Delta)
          sig_lev[] = 1-(sig_filt[]-Delta)/Delta;
        else
          sig_lev[] = 1;
    }
    boundary_level ({sig_lev}, l);
}

  // high pass filter
  for (int l = depth(); l >= 0; l--) {
    foreach_level (l)
      sig_lev[] = 1 - sig_lev[];
    boundary_level ({sig_lev}, l);  
  }

  /**
     BC for all fields */
  boundary (all);
}

/**
## Cleanup */

void trash_vars(){
  free (pol), pol = NULL;
  free (qol), qol = NULL;
  free (pom), pom = NULL;
  free (qom), qom = NULL;
  free (zetal), zetal = NULL;
  free (pofl), pofl = NULL;
  free (ppl), ppl = NULL;
  free (cl2m), cl2m = NULL;
  free (cm2l), cm2l = NULL;
  free (iBul), iBul = NULL;
  free (gpl), gpl = NULL;
  free (tmpl), tmpl = NULL;
  free (qosl), qosl = NULL;
  free(hl);

}

event cleanup (i = end, last) {
  trash_vars();
}