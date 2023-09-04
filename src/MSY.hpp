/* calculate single yield at F for spatial model across stocks and regions given stock-specific B-H/Ricker SR functions */
template<class Type>
struct sr_yield_spatial {
  // Data and parameter objects for calculation: 
  vector<Type> SR_a;
  vector<Type> SR_b; 
  vector<int> spawn_seasons;
  vector<int> spawn_regions;
  vector<int> fleet_regions; 
  matrix<int> fleet_seasons; 
  array<int> can_move; 
  vector<int> mig_type;
  vector<Type> fracyr_SSB;
  matrix<Type> selectivity;
  array<Type> log_M;
  array<Type> mu; 
  vector<Type> L;
  matrix<Type> mature;
  matrix<Type> waa_ssb; 
  matrix<Type> waa_catch; 
  vector<Type> fracyr_seasons;
  //vector<Type> SPR_weights; //how to weight stock-specific SSB/R for aggregate SSB/R.
  int age_specific; 
  vector<int> recruit_model;
  int small_dim;
  int trace=0;

  // Constructor 
  sr_yield_spatial(
  vector<Type> SR_a_, 
  vector<Type> SR_b_, 
  vector<int> spawn_seasons_,
  vector<int> spawn_regions_,
  vector<int> fleet_regions_, 
  matrix<int> fleet_seasons_, 
  array<int> can_move_, 
  vector<int> mig_type_,
  vector<Type> fracyr_SSB_,
  matrix<Type> selectivity_,
  array<Type> log_M_,
  array<Type> mu_, 
  vector<Type> L_,
  matrix<Type> mature_,
  matrix<Type> waa_ssb_, 
  matrix<Type> waa_catch_, 
  vector<Type> fracyr_seasons_,
  //vector<Type> SPR_weights_,
  int age_specific_, 
  vector<int> recruit_model_,
  int small_dim_,
  int trace_) :
    SR_a(SR_a_),
    SR_b(SR_b_),
    spawn_seasons(spawn_seasons_), 
    spawn_regions(spawn_regions_), 
    fleet_regions(fleet_regions_),
    fleet_seasons(fleet_seasons_),
    can_move(can_move_),
    mig_type(mig_type_),
    fracyr_SSB(fracyr_SSB_),
    selectivity(selectivity_),
    log_M(log_M_),
    mu(mu_),
    L(L_),
    mature(mature_),
    waa_ssb(waa_ssb_),
    waa_catch(waa_catch_),
    fracyr_seasons(fracyr_seasons_),
    //SPR_weights(SPR_weights_),
    age_specific(age_specific_),
    recruit_model(recruit_model_), 
    small_dim(small_dim_),
    trace(trace_) {}

  template <typename T> //I think this allows you to differentiate the function wrt whatever is after operator() on line below
  T operator()(vector<T> log_F) { //find such that it maximizes yield
    int n_stocks = log_M.dim(0);
    int n_regions = log_M.dim(1);
    int n_ages = log_M.dim(2);
    int n_fleets = selectivity.rows();
    if(trace) see("in sr_yield_spatial");
    //int n_seasons = can_move.dim(2);
    matrix<T> FAA(n_fleets,n_ages);
    FAA.setZero();
    for(int f = 0; f < n_fleets; f++) for(int a = 0; a < n_ages; a++) {
      FAA(f,a) = T(selectivity(f,a)) * exp(log_F(0));
    }
    if(trace) see(FAA);
    vector<T> fracyrssbT = fracyr_SSB.template cast<T>();
    array<T> logMbaseT(log_M.dim(0),log_M.dim(1),log_M.dim(2));
    for(int s = 0; s < n_stocks; s++) for(int r = 0; r < n_regions; r++) for(int a = 0; a < n_ages; a++){
      logMbaseT(s,r,a) = T(log_M(s,r,a));
    }
    if(trace) see(logMbaseT);
    array<T> muT(mu.dim(0),mu.dim(1),mu.dim(2),mu.dim(3),mu.dim(4));
    for(int s = 0; s < n_stocks; s++) for(int a = 0; a < n_ages; a++) for(int t = 0; t < mu.dim(2); t++) {
      for(int r = 0; r < n_regions; r++) for(int rr = 0; rr < n_regions; rr++) {
        muT(s,a,t,r,rr) = T(mu(s,a,t,r,rr));
      }
    }
    if(trace) see(muT);
    vector<T> LT = L.template cast<T>();
    matrix<T> matT = mature.template cast<T>();
    matrix<T> waassbT = waa_ssb.template cast<T>();
    matrix<T> waacatchT = waa_catch.template cast<T>();
    vector<T> fracyrseasonT = fracyr_seasons.template cast<T>();
    //stock-specific SSB/R
    array<T> SPR_sr = get_SPR(spawn_seasons, fleet_regions, fleet_seasons, can_move, mig_type, 
      fracyrssbT, 
      FAA, 
      logMbaseT, 
      muT, 
      LT, 
      matT, 
      waassbT, 
      fracyrseasonT, 
      0, small_dim, trace);
    if(trace) see("end get_SPR sr_yield_spatial");
    if(trace) see(SPR_sr);
    array<T> YPR_srf = get_YPR_srf(
      fleet_regions, 
      fleet_seasons, 
      can_move, 
      mig_type, 
      FAA, 
      logMbaseT, 
      muT,
      LT,
      waacatchT,
      fracyrseasonT,
      0, small_dim);
    if(trace) see("end get_YPR_srf sr_yield_spatial");
    if(trace) see(YPR_srf);

    //weighted-average of SSB/R returned
    vector<T> SPR(n_stocks), Y(n_stocks), R(n_stocks);
    SPR.setZero(); Y.setZero(); R.setZero();
    for(int s = 0; s < n_stocks; s++) {
      SPR(s) = SPR_sr(s,spawn_regions(s)-1,spawn_regions(s)-1);
      if(trace) see(SPR(s));
      if(recruit_model(s) == 3) R(s) = (T(SR_a(s)) - 1/SPR(s)) / T(SR_b(s)); //beverton-holt
      if(recruit_model(s) == 4) R(s) = log(T(SR_a(s)) * SPR(s))/(T(SR_b(s)) * SPR(s)); //ricker
      for(int r = 0; r < n_regions; r++) for(int f = 0; f < n_fleets; f ++) Y(s) += YPR_srf(s,r,f) * R(s);
    }
    if(trace) see(R);
    if(trace) see(SR_a);
    if(trace) see(SR_b);
    if(trace) see(Y);
    return Y.sum();
  }
};

/* calculate single yield at F for spatial model across stocks and regions given stock-specific B-H/Ricker SR functions */
template<class Type>
struct sr_yield_gradient_spatial {
  // Data and parameter objects for calculation: 
  vector<Type> SR_a;
  vector<Type> SR_b; 
  vector<int> spawn_seasons;
  vector<int> spawn_regions;
  vector<int> fleet_regions; 
  matrix<int> fleet_seasons; 
  array<int> can_move; 
  vector<int> mig_type;
  vector<Type> fracyr_SSB;
  matrix<Type> selectivity;
  array<Type> log_M;
  array<Type> mu; 
  vector<Type> L;
  matrix<Type> mature;
  matrix<Type> waa_ssb; 
  matrix<Type> waa_catch; 
  vector<Type> fracyr_seasons;
  //vector<Type> SPR_weights; //how to weight stock-specific SSB/R for aggregate SSB/R.
  int age_specific; 
  vector<int> recruit_model;
  int small_dim;
  int trace=0;

  // Constructor 
  sr_yield_gradient_spatial(
  vector<Type> SR_a_, 
  vector<Type> SR_b_, 
  vector<int> spawn_seasons_,
  vector<int> spawn_regions_,
  vector<int> fleet_regions_, 
  matrix<int> fleet_seasons_, 
  array<int> can_move_, 
  vector<int> mig_type_,
  vector<Type> fracyr_SSB_,
  matrix<Type> selectivity_,
  array<Type> log_M_,
  array<Type> mu_, 
  vector<Type> L_,
  matrix<Type> mature_,
  matrix<Type> waa_ssb_, 
  matrix<Type> waa_catch_, 
  vector<Type> fracyr_seasons_,
  //vector<Type> SPR_weights_,
  int age_specific_, 
  vector<int> recruit_model_,
  int small_dim_,
  int trace_) :
    SR_a(SR_a_),
    SR_b(SR_b_),
    spawn_seasons(spawn_seasons_), 
    spawn_regions(spawn_regions_), 
    fleet_regions(fleet_regions_),
    fleet_seasons(fleet_seasons_),
    can_move(can_move_),
    mig_type(mig_type_),
    fracyr_SSB(fracyr_SSB_),
    selectivity(selectivity_),
    log_M(log_M_),
    mu(mu_),
    L(L_),
    mature(mature_),
    waa_ssb(waa_ssb_),
    waa_catch(waa_catch_),
    fracyr_seasons(fracyr_seasons_),
    //SPR_weights(SPR_weights_),
    age_specific(age_specific_),
    recruit_model(recruit_model_), 
    small_dim(small_dim_),
    trace(trace_) {}

  template <typename T> //I think this allows you to differentiate the function wrt whatever is after operator() on line below
  vector<T> operator()(vector<T> log_F) { //find such that it maximizes yield
    int n_stocks = log_M.dim(0);
    int n_regions = log_M.dim(1);
    int n_ages = log_M.dim(2);
    int n_fleets = selectivity.rows();
    if(trace) see("in sr_yield_gradient_spatial");
    //int n_seasons = can_move.dim(2);
    matrix<T> FAA(n_fleets,n_ages);
    FAA.setZero();
    for(int f = 0; f < n_fleets; f++) for(int a = 0; a < n_ages; a++) {
      FAA(f,a) = T(selectivity(f,a)) * exp(log_F(0));
    }
    if(trace) see(FAA);
    vector<T> fracyrssbT = fracyr_SSB.template cast<T>();
    array<T> logMbaseT(log_M.dim(0),log_M.dim(1),log_M.dim(2));
    for(int s = 0; s < n_stocks; s++) for(int r = 0; r < n_regions; r++) for(int a = 0; a < n_ages; a++){
      logMbaseT(s,r,a) = T(log_M(s,r,a));
    }
    if(trace) see(logMbaseT);
    array<T> muT(mu.dim(0),mu.dim(1),mu.dim(2),mu.dim(3),mu.dim(4));
    for(int s = 0; s < n_stocks; s++) for(int a = 0; a < n_ages; a++) for(int t = 0; t < mu.dim(2); t++) {
      for(int r = 0; r < n_regions; r++) for(int rr = 0; rr < n_regions; rr++) {
        muT(s,a,t,r,rr) = T(mu(s,a,t,r,rr));
      }
    }
    if(trace) see(muT);
    vector<T> LT = L.template cast<T>();
    matrix<T> matT = mature.template cast<T>();
    matrix<T> waassbT = waa_ssb.template cast<T>();
    matrix<T> waacatchT = waa_catch.template cast<T>();
    vector<T> fracyrseasonT = fracyr_seasons.template cast<T>();
    //stock-specific SSB/R
    array<T> SPR_sr = get_SPR(spawn_seasons, fleet_regions, fleet_seasons, can_move, mig_type, 
      fracyrssbT, 
      FAA, 
      logMbaseT, 
      muT, 
      LT, 
      matT, 
      waassbT, 
      fracyrseasonT, 
      0, small_dim, trace);
    if(trace) see("end get_SPR sr_yield_gradient_spatial");
    if(trace) see(SPR_sr);
    array<T> YPR_srf = get_YPR_srf(
      fleet_regions, 
      fleet_seasons, 
      can_move, 
      mig_type, 
      FAA, 
      logMbaseT, 
      muT,
      LT,
      waacatchT,
      fracyrseasonT,
      0, small_dim);
    if(trace) see("end get_YPR_srf sr_yield_gradient_spatial");
    if(trace) see(YPR_srf);

    //weighted-average of SSB/R returned
    vector<T> SPR(n_stocks), Y(n_stocks), R(n_stocks);
    SPR.setZero(); Y.setZero(); R.setZero();
    for(int s = 0; s < n_stocks; s++) {
      SPR(s) = SPR_sr(s,spawn_regions(s)-1,spawn_regions(s)-1);
      if(trace) see(SPR(s));
      if(recruit_model(s) == 3) R(s) = (T(SR_a(s)) - 1/SPR(s)) / T(SR_b(s)); //beverton-holt
      if(recruit_model(s) == 4) R(s) = log(T(SR_a(s)) * SPR(s))/(T(SR_b(s)) * SPR(s)); //ricker
      for(int r = 0; r < n_regions; r++) for(int f = 0; f < n_fleets; f ++) Y(s) += YPR_srf(s,r,f) * R(s);
    }
    if(trace) see(R);
    if(trace) see(SR_a);
    if(trace) see(SR_b);
    if(trace) see(Y);
    return Y;
  }
};

//takes a single year of values for inputs (reduce dimensions appropriately)
//returns just the "solved" log_Fmsy value
template <class Type>
Type get_FMSY(vector<Type> a, vector<Type> b, vector<int> spawn_seasons, vector<int> spawn_regions, vector<int> fleet_regions,
  matrix<int> fleet_seasons, array<int> can_move, vector<int> mig_type, vector<Type> ssbfrac, matrix<Type> sel, array<Type> log_M, array<Type> mu, 
  vector<Type> L, matrix<Type> mat,  matrix<Type> waassb, matrix<Type> waacatch,
  vector<Type> fracyr_seasons, vector<int> recruit_model, int small_dim, Type F_init, int n_iter, int trace = 0) {
  int n = n_iter;
  vector<Type> log_FMSY_i(1);
  vector<Type> log_FMSY_iter(n);
  log_FMSY_iter.fill(log(F_init)); //starting value
  //vector<Type> a = exp(log_a);
  //vector<Type> b = exp(log_b);
  sr_yield_spatial<Type> srY(a, b, spawn_seasons, spawn_regions, fleet_regions, fleet_seasons, can_move, mig_type, 
    ssbfrac, sel, log_M, mu, L, mat, waassb, waacatch, fracyr_seasons, 0, recruit_model, small_dim, trace);
  for (int i=0; i<n-1; i++) {
    log_FMSY_i(0) = log_FMSY_iter(i);
    vector<Type> grad_sr_yield = autodiff::gradient(srY,log_FMSY_i);
    matrix<Type> hess_sr_yield = autodiff::hessian(srY,log_FMSY_i);
    log_FMSY_iter(i+1) = log_FMSY_iter(i) - grad_sr_yield(0)/hess_sr_yield(0,0);
  }
  Type FMSY = exp(log_FMSY_iter(n-1));
  return FMSY;
}

//returns annual values of 
template <class Type>
vector<Type> get_log_FMSY(array<Type> FAA, vector<int> fleet_regions, matrix<int> fleet_seasons, 
  vector<int> spawn_seasons, vector<int> spawn_regions, array<int> can_move, vector<int> mig_type, vector<Type> fracyr_seasons, 
  vector<int> which_F_age, vector<int> recruit_model, matrix<Type> log_a, matrix<Type> log_b, 
  matrix<Type> fracyr_SSB, array<Type> log_M, array<Type> mu, matrix<Type> L, array<Type> waa_ssb, array<Type> waa_catch, 
  array<Type> mature, int small_dim, vector<Type> FMSY_init, int trace = 0){

  int n_years_pop = waa_ssb.dim(1);
  vector<int> yvec(1);
  vector<Type> log_FMSY(n_years_pop);

  for(int y = 0; y < n_years_pop; y++){
    yvec(0) = y;
    if(trace) see(y);
    matrix<Type> waa_ssb_y = get_matrix_y(waa_ssb, y);
    matrix<Type> waa_catch_y = get_matrix_y(waa_catch, y);
    matrix<Type> mature_y = get_matrix_y(mature, y);
    matrix<Type> sel_y = get_avg_fleet_sel(FAA, yvec, which_F_age(y));
    if(trace) see(sel_y);
    vector<Type> L_y = L.row(y);
    if(trace) see(L_y);
    array<Type> log_M_y = get_log_M_y(y,log_M);
    if(trace) see(log_M_y);
    array<Type> mu_y = get_mu_y(y,mu);
    if(trace) see(mu_y);
    if(trace) see(mu_y.dim);
    vector<Type> a_y = exp(vector<Type> (log_a.row(y)));
    vector<Type> b_y = exp(vector<Type> (log_b.row(y)));

    Type FMSY = get_FMSY(a_y, b_y, spawn_seasons, spawn_regions, fleet_regions, fleet_seasons, can_move, mig_type, 
          vector<Type> (fracyr_SSB.row(y)), sel_y, log_M_y, mu_y, L_y, mature_y,  waa_ssb_y, waa_catch_y, fracyr_seasons, recruit_model, small_dim, 
          FMSY_init(y), 10, trace);
    log_FMSY(y) = log(FMSY);
    if(trace) see(log_FMSY(y));
  }

  return log_FMSY;
}

template <class Type>
vector< matrix <Type> > get_MSY_res(
  vector<int> recruit_model,
  matrix<Type> log_SR_a,
  matrix<Type> log_SR_b,
  array<Type> log_M, 
  array<Type> FAA, 
  vector<int> spawn_seasons,  
  vector<int> spawn_regions,
  vector<int> fleet_regions, 
  matrix<int> fleet_seasons,
  vector<Type> fracyr_seasons,
  array<int> can_move,
  array<int> must_move,
  vector<int> mig_type,
  array<Type> trans_mu_base, 
  matrix<Type> L,
  int which_F_age, array<Type> waa, vector<int> waa_pointer_ssb, 
  vector<int> waa_pointer_fleets,
  array<Type> mature, matrix<Type> fracyr_SSB, Type F_init, 
  vector<int> years_M, vector<int> years_mu, vector<int> years_L, vector<int> years_mat, vector<int> years_sel, 
  vector<int> years_waa_ssb, vector<int> years_waa_catch, vector<int> years_SR_ab,
  int small_dim, int trace = 0, int n_iter = 10) {
  if(trace) see("inside get_SPR_res");
  int n = n_iter;
  int n_stocks = log_M.dim(0);
  int n_fleets = FAA.dim(0);
  int n_regions = can_move.dim(2);
  int n_seasons = fleet_seasons.cols();
  int n_ages = log_M.dim(3);
  if(trace) see(n_ages);
  matrix<Type> waa_ssb(n_stocks,n_ages);
  matrix<Type> waa_catch(n_fleets,n_ages);
  matrix<Type> sel(n_fleets, n_ages);
  array<Type> M(n_stocks,n_regions,n_ages);
  array<Type> mu_avg(n_stocks,n_seasons,n_ages, n_regions, n_regions);
  vector<Type> L_avg(n_regions);
  matrix<Type> mat(n_stocks, n_ages);
  vector<Type> ssbfrac(n_stocks); //R(n_stocks); R.setZero(); 
  vector<Type> SR_a_avg(n_stocks), SR_b_avg(n_stocks);

  waa_ssb.setZero(); waa_catch.setZero(); sel.setZero(); M.setZero(); mu_avg.setZero(); L_avg.setZero(); 
  mat.setZero(); ssbfrac.setZero();
  SR_a_avg.setZero(); SR_b_avg.setZero();
  //get average inputs over specified years

  matrix<Type> SR_ab_avg = get_avg_SR_ab(log_SR_a, log_SR_b, years_SR_ab, 0);
  // for(int y = 0; y < years_SR_ab.size(); y++) for(int s = 0; s < n_stocks; s++) {
  //   SR_a_avg(s) += exp(log_SR_a(y,s))/Type(years_SR_ab.size());
  //   SR_b_avg(s) += exp(log_SR_b(y,s))/Type(years_SR_ab.size());
  // }
  if(trace) see(SR_ab_avg);

  L_avg = get_avg_L(L, years_L, 0);
  //for(int y = 0; y < years_L.size(); y++) for(int i = 0; i < n_regions; i++) L_avg(i) += L(years_L(y),i)/Type(years_L.size());
  if(trace) see(L_avg);
  array<Type> log_avg_M = get_avg_M(log_M, years_M, 1);
  if(trace) see(log_avg_M);

  if(n_regions>1) mu_avg = get_avg_mu(trans_mu_base,years_mu,mig_type, can_move, must_move);
  if(trace) see(mu_avg);

  mat = get_avg_mat(mature,years_mat);
  if(trace) see(mat);
  ssbfrac = get_avg_ssbfrac(fracyr_SSB,years_waa_ssb);
  if(trace) see(ssbfrac);
  
  waa_ssb = get_avg_waa(waa, years_waa_ssb, waa_pointer_ssb);
  if(trace) see(waa_ssb);
  waa_catch = get_avg_waa(waa, years_waa_catch, waa_pointer_fleets);
  if(trace) see(waa_catch);

  matrix<Type> FAA_avg = get_avg_FAA(FAA,years_sel,0);
  vector<Type> FAA_avg_tot = FAA_avg.colwise().sum();
  if(trace) see(FAA_avg_tot);

  //which_F_age needs to be set appropriately by user or by wham:::check_which_F_age.
  if(trace) see(which_F_age);
  sel = FAA_avg/FAA_avg_tot(which_F_age-1);
  if(trace) see(sel);

  vector<Type> log_FMSY_i(1);
  matrix<Type> log_FMSY_iter(n,1);
  log_FMSY_iter(0,0) = log(F_init);

  if(trace) see(log_FMSY_iter(0,0));
  
  sr_yield_spatial<Type> srY(SR_ab_avg.col(0),SR_ab_avg.col(1),spawn_seasons, spawn_regions, fleet_regions, fleet_seasons, can_move, mig_type, 
    ssbfrac, sel, log_avg_M, mu_avg, L_avg, mat, waa_ssb, waa_catch, fracyr_seasons, 0, recruit_model, small_dim, 0);
  if(trace) see("after spr_F_spatial sprF defined");
  for(int i=0; i<n-1; i++) {
    if(trace) see(i);
    log_FMSY_i(0) = log_FMSY_iter(i,0);
    if(trace) see(srY(log_FMSY_i));
    vector<Type> grad_srY = autodiff::gradient(srY,log_FMSY_i);
    if(trace) see(grad_srY);

    matrix<Type> hess_srY = autodiff::hessian(srY,log_FMSY_i);
    if(trace) see(hess_srY);
    log_FMSY_iter(i+1,0) = log_FMSY_iter(i,0) - grad_srY(0)/hess_srY(0,0);
    if(trace) see(log_FMSY_iter(i,0));
  }
  matrix<Type> FAA_MSY = exp(log_FMSY_iter(n-1,0)) * sel;
  if(trace) see(FAA_MSY);
  matrix<Type> log_FAA_MSY(n_fleets+1,n_ages);
  log_FAA_MSY.setZero();
  for(int a = 0; a < n_ages; a++){
    for(int f = 0; f < n_fleets; f++) {
      log_FAA_MSY(f,a) = log(FAA_MSY(f,a));
      log_FAA_MSY(n_fleets, a) += FAA_MSY(f,a); //summing, not log yet
    }
    log_FAA_MSY(n_fleets, a) = log(log_FAA_MSY(n_fleets, a)); //log it
  }
  if(trace) see(log_FAA_MSY);
  array<Type> SPR_all = get_SPR(spawn_seasons, fleet_regions, fleet_seasons, can_move, mig_type, ssbfrac, FAA_MSY, log_avg_M, mu_avg, L_avg, 
    mat, waa_ssb, fracyr_seasons, 0, small_dim);
  if(trace) see(SPR_all);

  array<Type> YPR_srf = get_YPR_srf(fleet_regions, fleet_seasons, can_move, mig_type, FAA_MSY, log_avg_M, mu_avg, L_avg, waa_catch, 
    fracyr_seasons, 0, small_dim); //n_stocks x n_regions x n_fleets (should be 0 for regions not fleet_regions(f)-1)
  if(trace) see(YPR_srf);
  
  matrix<Type> log_SPR_MSY(n_stocks+1,1), log_R_MSY(n_stocks+1,1), log_MSY(n_fleets+1,n_stocks+1), log_SSB_MSY(n_stocks+1,1), log_YPR_MSY(n_fleets+1,n_stocks+1); 
  log_SPR_MSY.setZero(); log_MSY.setZero(); log_R_MSY.setZero(); log_SSB_MSY.setZero(); log_YPR_MSY.setZero();
  for(int s = 0; s < n_stocks; s++) {
    log_SPR_MSY(s,0) = log(SPR_all(s,spawn_regions(s)-1,spawn_regions(s)-1));
    if(recruit_model(s) == 3) log_R_MSY(s,0) = log((SR_a_avg(s) - 1/exp(log_SPR_MSY(s,0))) / SR_b_avg(s)); //b-h
    if(recruit_model(s) == 4) log_R_MSY(s,0) = log(log(SR_a_avg(s)) + log_SPR_MSY(s,0)) - log(SR_b_avg(s)) - log_SPR_MSY(s,0); //ricker
    log_SSB_MSY(s,0) = log_R_MSY(s,0) + log_SPR_MSY(s,0);
    log_R_MSY(n_stocks,0) += exp(log_R_MSY(s,0)); //not logged
    log_SSB_MSY(n_stocks,0) += exp(log_SSB_MSY(s,0)); //not logged
    for(int f = 0; f < n_fleets; f++) {
      //NOTE: If stock s cannot ever be in fleet_region(f)-1 then this will give log(0);
      log_YPR_MSY(f,s) = log(YPR_srf(s,fleet_regions(f)-1,f));
      //log_YPR_MSY(n_stocks*n_fleets + s) += YPR_srf(s,fleet_regions(f)-1,f); //not logged yet
      log_YPR_MSY(f,s) = log(YPR_srf(s,fleet_regions(f)-1,f));
      log_MSY(f,s) += exp(log_R_MSY(s,0)) * YPR_srf(s,fleet_regions(f)-1,f); //not logged yet, can be 0
      log_MSY(n_fleets,s) += log_MSY(f,s); //not logged yet
      log_MSY(f,n_stocks) += log_MSY(f,s); //not logged yet
      log_MSY(n_fleets,n_stocks) += log_MSY(f,s); //total yield across stocks, fleets not logged yet
    }
    log_MSY(n_fleets,s) = log(log_MSY(n_fleets,s)); //logged now.
    log_YPR_MSY(n_fleets,s) = log_MSY(n_fleets,s) - log_R_MSY(s,0); //total YPR for stock s 
    //log_YPR_MSY(n_stocks*n_fleets + s) = log(log_YPR_MSY(n_stocks*n_fleets + s)); //log total YPR (across fleets) for stock s
  }
  if(trace) see(log_MSY);
  log_SSB_MSY(n_stocks,0) = log(log_SSB_MSY(n_stocks,0)); //logged now
  log_R_MSY(n_stocks,0) = log(log_R_MSY(n_stocks,0)); //logged now
  log_SPR_MSY(n_stocks,0) = log_SSB_MSY(n_stocks,0) - log_R_MSY(n_stocks,0);
  for(int f = 0; f < n_fleets; f++) {
    log_MSY(f,n_stocks) = log(log_MSY(f,n_stocks)); //logged now.
    log_YPR_MSY(f,n_stocks) = log_MSY(f,n_stocks) - log_R_MSY(n_stocks,0); //total YPR for fleet f 
  }
  log_MSY(n_fleets,n_stocks) = log(log_MSY(n_fleets,n_stocks)); //logged now.
  log_YPR_MSY(n_fleets,n_stocks) = log_MSY(n_fleets,n_stocks) - log_R_MSY(n_stocks,0); //logged now
  //not done with the next line yet!!!!!!!!
  //log_YPR_MSY(n_stocks*n_fleets + s)
  vector< matrix<Type> > res(6); 
  res(0) = log_SSB_MSY;
  res(1) = log_R_MSY;
  res(2) = log_SPR_MSY; //log SSB/R at FMSY
  res(3) = log_FAA_MSY; // log_FAA at MSY by fleet and across fleets
  res(4) = log_MSY; 
  res(5) = log_YPR_MSY;
  res(6) = log_FMSY_iter; //last value is Fmsy across ages and fleets
  if(trace) see("end get_MSY_res")
  return res;
}

template <class Type>
vector< array <Type> > get_annual_MSY_res(
  vector<int> recruit_model,
  matrix<Type> log_SR_a,
  matrix<Type> log_SR_b,
  array<Type> log_M, 
  array<Type> FAA, 
  vector<int> spawn_seasons,  
  vector<int> spawn_regions,
  vector<int> fleet_regions, 
  matrix<int> fleet_seasons,
  vector<Type> fracyr_seasons,
  array<int> can_move,
  array<int> must_move,
  vector<int> mig_type,
  array<Type> trans_mu_base, 
  matrix<Type> L,
  vector<int> which_F_age, array<Type> waa, vector<int> waa_pointer_ssb, 
  vector<int> waa_pointer_fleets,
  array<Type> mature, matrix<Type> fracyr_SSB, vector<Type> F_init, 
  int small_dim, int trace = 0, int n_iter = 10) {
  
  int ny = which_F_age.size();
  int n_fleets = waa_pointer_fleets.size();
  int n_stocks = waa_pointer_ssb.size();
  int n_ages = mature.dim(2);

  array<Type> log_SSB_MSY(ny,n_stocks+1); 
  array<Type> log_R_MSY(ny,n_stocks+1); 
  array<Type> log_SPR_MSY(ny,n_stocks+1); 
  array<Type> log_FAA_MSY(n_fleets+1,ny,n_ages); 
  array<Type> log_MSY(n_fleets+1,n_stocks+1,ny); 
  array<Type> log_YPR_MSY(n_fleets+1,n_stocks+1,ny); 
  array<Type> log_FMSY_iter(ny,n_iter);

  vector<int> yvec(1);

  for(int y = 0; y < ny; y++){
    yvec(0) = y;

    vector<matrix<Type>> MSY_res_y = get_MSY_res(
      recruit_model, log_SR_a, log_SR_b, log_M, FAA, spawn_seasons, spawn_regions,
      fleet_regions, fleet_seasons, fracyr_seasons, can_move, must_move, mig_type,
      trans_mu_base, L, which_F_age(y), 
      waa, waa_pointer_ssb, waa_pointer_fleets,
      mature, fracyr_SSB, F_init(y), 
      yvec, yvec, yvec, yvec, yvec, yvec, yvec, yvec,
      small_dim, trace, n_iter);
    for(int s = 0; s <= n_stocks; s++) {
      log_SSB_MSY(y,s) = MSY_res_y(0)(s,0);
      log_R_MSY(y,s) = MSY_res_y(1)(s,0);
      log_SPR_MSY(y,s) = MSY_res_y(2)(s,0);
      for(int f = 0; f <= n_fleets; f++) {
        for(int a = 0; a < n_ages; a++) log_FAA_MSY(f,y,a) = MSY_res_y(3)(f,a);
        log_MSY(f,s,y) = MSY_res_y(4)(f,s);
        log_YPR_MSY(s,f,y) = MSY_res_y(5)(f,s);
      }
    }
    for(int i = 0; i < n_iter; i++) log_FMSY_iter(y,i) = MSY_res_y(6)(i);
  }
  vector< array <Type>> res(7);
  res(0) = log_SSB_MSY;
  res(1) = log_R_MSY;
  res(2) = log_SPR_MSY; //log SSB/R at FMSY
  res(3) = log_FAA_MSY; // log_FAA at MSY by fleet and across fleets
  res(4) = log_MSY; 
  res(5) = log_YPR_MSY;
  res(6) = log_FMSY_iter; //last value is Fmsy across ages and fleets

  return res;
}