/**
 * Copyright 2017, IBM.
 *
 * This source code is licensed under the Apache License, Version 2.0 found in
 * the LICENSE.txt file in the root directory of this source tree.
 */

#ifndef _qubitvector_qv_state_hpp
#define _qubitvector_qv_state_hpp

#include <algorithm>
#define _USE_MATH_DEFINES
#include <math.h>

#include "framework/utils.hpp"
#include "framework/json.hpp"
#include "base/state.hpp"
#include "qubitvector.hpp"


namespace AER {
namespace QubitVector {
  
// Allowed gates enum class
enum class Gates {
  u0, u1, u2, u3, id, x, y, z, h, s, sdg, t, tdg, // single qubit
  cx, cz, rzz, swap, // two qubit
  ccx // three qubit
};

/*******************************************************************************
 *
 * QubitVector State class
 *
 ******************************************************************************/

template <class statevector_t = cvector_t>
class State : public Base::State<QV::QubitVector<statevector_t>> {
public:
  using BaseState = Base::State<QV::QubitVector<statevector_t>>;

  State() = default;
  virtual ~State() = default;

  //-----------------------------------------------------------------------
  // Base class overrides
  //-----------------------------------------------------------------------

  // Return the set of qobj instruction types supported by the State
  inline virtual std::unordered_set<Operations::OpType> allowed_ops() const override {
    return std::unordered_set<Operations::OpType>({
      Operations::OpType::gate,
      Operations::OpType::measure,
      Operations::OpType::reset,
      Operations::OpType::barrier,
      Operations::OpType::matrix,
      Operations::OpType::snapshot_state,
      Operations::OpType::snapshot_probs,
      Operations::OpType::snapshot_pauli,
      Operations::OpType::snapshot_matrix,
      Operations::OpType::kraus
    });
  }

  // Return the set of qobj gate instruction names supported by the State
  inline virtual stringset_t allowed_gates() const override {
    return {"U", "CX", "u0", "u1", "u2", "u3", "cx", "cz", "swap",
            "id", "x", "y", "z", "h", "s", "sdg", "t", "tdg", "ccx"};
  }

  // Applies an operation to the state class.
  // This should support all and only the operations defined in
  // allowed_operations.
  virtual void apply_op(const Operations::Op &op) override;

  // Initializes an n-qubit state to the all |0> state
  virtual void initialize(uint_t num_qubits) override;

  // Returns the required memory for storing an n-qubit state in megabytes.
  // For this state the memory is indepdentent of the number of ops
  // and is approximately 16 * 1 << num_qubits bytes
  virtual uint_t required_memory_mb(uint_t num_qubits,
                                    const std::vector<Operations::Op> &ops) override;

  // Load the threshold for applying OpenMP parallelization
  // if the controller/engine allows threads for it
  // Config: {"omp_qubit_threshold": 14}
  // TODO: Check optimal default value for a desktop i7 CPU
  virtual void set_config(const json_t &config) override;

  // Measure qubits and return a list of outcomes [q0, q1, ...]
  // If a state subclass supports this function it then "measure" 
  // should be contained in the set returned by the 'allowed_ops'
  // method.
  virtual reg_t apply_measure(const reg_t& qubits) override;

  // Return vector of measure probabilities for specified qubits
  // If a state subclass supports this function it then "measure" 
  // should be contained in the set returned by the 'allowed_ops'
  // method.
  virtual rvector_t measure_probs(const reg_t &qubits) const override;

  // Sample n-measurement outcomes without applying the measure operation
  // to the system state.
  virtual std::vector<reg_t>
  sample_measure(const reg_t& qubits, uint_t shots = 1) override;

  // Return the complex expectation value for an observable operator
  virtual double pauli_observable_value(const reg_t& qubits, 
                                        const std::string &pauli) const override;

  // Return the complex expectation value for an observable operator
  virtual complex_t matrix_observable_value(const Operations::Op &op) const override;

protected:

  const static stringmap_t<Gates> gateset;

  //-----------------------------------------------------------------------
  // Config Settings
  //-----------------------------------------------------------------------

  int omp_qubit_threshold_ = 14; // Test this on desktop i7 to find best setting 

  //-----------------------------------------------------------------------
  // Apply Gates
  //-----------------------------------------------------------------------

  // Applies a Gate operation to the state class.
  // This should support all and only the operations defined in
  // allowed_operations.
  void apply_gate(const Operations::Op &op);

  //-----------------------------------------------------------------------
  // Apply Matrices
  //-----------------------------------------------------------------------

  // Apply a matrix to given qubits (identity on all other qubits)
  void apply_matrix(const reg_t &qubits, const cmatrix_t & mat);

  // Apply a vectorized matrix to given qubits (identity on all other qubits)
  void apply_matrix(const reg_t &qubits, const cvector_t & vmat);

  //-----------------------------------------------------------------------
  // Apply Kraus Noise
  //-----------------------------------------------------------------------

  void apply_kraus(const reg_t &qubits, const std::vector<cmatrix_t> &krausops);

  //-----------------------------------------------------------------------
  // Measurement and Reset Helpers
  //-----------------------------------------------------------------------

  void apply_reset(const reg_t &qubits, const uint_t reset_state = 0);

  // Sample the measurement outcome for qubits
  // return a pair (m, p) of the outcome m, and its corresponding
  // probability p.
  // Outcome is given as an int: Eg for two-qubits {q0, q1} we have
  // 0 -> |q1 = 0, q0 = 0> state
  // 1 -> |q1 = 0, q0 = 1> state
  // 2 -> |q1 = 1, q0 = 0> state
  // 3 -> |q1 = 1, q0 = 1> state
  std::pair<uint_t, double> sample_measure_with_prob(const reg_t &qubits);


  void measure_reset_update(const std::vector<uint_t> &qubits,
                            const uint_t final_state,
                            const uint_t meas_state,
                            const double meas_prob);

  //-----------------------------------------------------------------------
  // 1-Qubit Gates
  //-----------------------------------------------------------------------
  
  void apply_gate_u3(const uint_t qubit, const double theta, const double phi,
                       const double lambda);

  // Optimize phase gate with diagonal [1, phase]
  void apply_gate_phase(const uint_t qubit, const complex_t phase);


  //-----------------------------------------------------------------------
  // Generate matrices
  //-----------------------------------------------------------------------

  cvector_t waltz_vectorized_matrix(double theta, double phi, double lambda);

  cvector_t rzz_diagonal_matrix(double lambda);

};


//============================================================================
// Implementation: Allowed ops and gateset
//============================================================================

template <class statevec_t>
const stringmap_t<Gates> State<statevec_t>::gateset({
  // Single qubit gates
  {"id", Gates::id},   // Pauli-Identity gate
  {"x", Gates::x},    // Pauli-X gate
  {"y", Gates::y},    // Pauli-Y gate
  {"z", Gates::z},    // Pauli-Z gate
  {"s", Gates::s},    // Phase gate (aka sqrt(Z) gate)
  {"sdg", Gates::sdg}, // Conjugate-transpose of Phase gate
  {"h", Gates::h},    // Hadamard gate (X + Z / sqrt(2))
  {"t", Gates::t},    // T-gate (sqrt(S))
  {"tdg", Gates::tdg}, // Conjguate-transpose of T gate
  // Waltz Gates
  {"u0", Gates::u0},  // idle gate in multiples of X90
  {"u1", Gates::u1},  // zero-X90 pulse waltz gate
  {"u2", Gates::u2},  // single-X90 pulse waltz gate
  {"u3", Gates::u3},  // two X90 pulse waltz gate
  // Two-qubit gates
  {"cx", Gates::cx},  // Controlled-X gate (CNOT)
  {"cz", Gates::cz},  // Controlled-Z gate
  {"rzz", Gates::rzz}, // ZZ-rotation gate
  {"swap", Gates::swap}, // SWAP gate
  // Three-qubit gates
  {"ccx", Gates::ccx}  // Controlled-CX gate (Toffoli)
}); 

//============================================================================
// Implementation: Base class method overrides
//============================================================================

template <class statevec_t>
uint_t State<statevec_t>::required_memory_mb(uint_t num_qubits,
                                 const std::vector<Operations::Op> &ops) {
  // An n-qubit state vector as 2^n complex doubles
  // where each complex double is 16 bytes
  (void)ops; // avoid unused variable compiler warning
  uint_t shift_mb = std::max<int_t>(0, num_qubits + 4 - 20);
  uint_t mem_mb = 1ULL << shift_mb;
  return mem_mb;
}

template <class statevec_t>
void State<statevec_t>::set_config(const json_t &config) {
  
  // Set OMP threshold for state update functions
  JSON::get_value(omp_qubit_threshold_, "omp_qubit_threshold", config);
  
  // Set the sample measure indexing size
  int index_size;
  if (JSON::get_value(index_size, "sample_measure_index_size", config)) {
    BaseState::data_.set_sample_measure_index_size(index_size);
  };

  // Enable sorted gate optimzations
  bool gate_opt = false;
  JSON::get_value(gate_opt, "gate_optimization", config);
  if (gate_opt)
    BaseState::data_.enable_gate_opt();
}

template <class statevec_t>
void State<statevec_t>::initialize(uint_t num_qubits) {
  // reset state std::vector to default state
  BaseState::data_ = QV::QubitVector<statevec_t>(num_qubits);

  // Set maximum threads for QubitVector parallelization
  BaseState::data_.set_omp_threshold(omp_qubit_threshold_);
  if (BaseState::threads_ > 0)
    BaseState::data_.set_omp_threads(BaseState::threads_); // set allowed OMP threads in qubitvector
  
  BaseState::data_.initialize(); // initialize qubit vector to all |0> state
}

template <class statevec_t>
reg_t State<statevec_t>::apply_measure(const reg_t &qubits) {
  // Actual measurement outcome
  const auto meas = sample_measure_with_prob(qubits);
  // Implement measurement update
  measure_reset_update(qubits, meas.first, meas.first, meas.second);
  return Utils::int2reg(meas.first, 2, qubits.size());
}

template <class statevec_t>
rvector_t State<statevec_t>::measure_probs(const reg_t &qubits) const {
  return BaseState::data_.probabilities(qubits);
}

template <class statevec_t>
std::vector<reg_t>
State<statevec_t>::sample_measure(const reg_t &qubits, uint_t shots) {
  std::vector<double> rnds;
  rnds.reserve(shots);
  for (uint_t i = 0; i < shots; ++i)
    rnds.push_back(BaseState::rng_.rand(0, 1));

  auto allbit_samples = BaseState::data_.sample_measure(rnds);

  std::vector<reg_t> samples;
  samples.reserve(shots);

  for (int_t val : allbit_samples) {
    reg_t allbit_sample = Utils::int2reg(val, 2, qubits.size());
    reg_t sample;
    sample.reserve(qubits.size());
    for (uint_t qubit : qubits)
      sample.push_back(allbit_sample[qubit]);
    samples.push_back(sample);
  }

  return samples;
}

template <class statevec_t>
double State<statevec_t>::pauli_observable_value(const reg_t& qubits,
                                              const std::string &pauli) const {

  // Copy the quantum state;
  QV::QubitVector<statevec_t> data_copy = BaseState::data_;
  // Apply each pauli operator as a gate to the corresponding qubit
  for (size_t pos=0; pos < qubits.size(); ++pos) {
    switch (pauli[pos]) {
      case 'I':
        break;
      case 'X':
        data_copy.apply_x(qubits[pos]);
        break;
      case 'Y':
        data_copy.apply_y(qubits[pos]);
        break;
      case 'Z':
        data_copy.apply_z(qubits[pos]);
        break;
      default: {
        std::stringstream msg;
        msg << "QubitVectorState::invalid Pauli string \'" << pauli[pos] << "\'.";
        throw std::invalid_argument(msg.str());
      }
    }
  }
  // Pauli expecation values should always be real for a state
  return std::real(data_copy.inner_product(BaseState::data_));
}

template <class statevec_t>
complex_t State<statevec_t>::matrix_observable_value(const Operations::Op &op) const {


  // Check empty edge case
  if (op.params_mat_obs.empty()) {
    throw std::invalid_argument("Invalid matrix snapshot (components are empty).");
  }
  
  complex_t expval = 0.;
  for (const auto &param : op.params_mat_obs) {
    const auto& coeff = std::get<0>(param);
    const auto& qubits = std::get<1>(param);
    const auto& matrices = std::get<2>(param);
    QV::QubitVector<statevec_t> data_copy = BaseState::data_; // Copy the quantum state
    // Apply each qubit subset gate
    for (size_t pos=0; pos < qubits.size(); ++pos) {
      const cmatrix_t &mat = matrices[pos];
      cvector_t vmat = (mat.GetColumns() == 1)
        ? Utils::vectorize_matrix(Utils::projector(Utils::vectorize_matrix(mat))) // projector case
        : Utils::vectorize_matrix(mat); // diagonal or square matrix case
      if (vmat.size() == 1ULL << qubits[pos].size()) {
        data_copy.apply_diagonal_matrix(qubits[pos], vmat); // TODO CHECK FOR diagonal
      } else {
        data_copy.apply_matrix(qubits[pos], vmat);
      }
      
    }
    expval += data_copy.inner_product(BaseState::data_) * coeff; // add component to expectation value
  }
  return expval;
}

template <class statevec_t>
void State<statevec_t>::apply_op(const Operations::Op &op) {

  // Check if op is measure, reset, mat or kraus
  switch (op.type) {
    case Operations::OpType::barrier:
      break;
    case Operations::OpType::reset:
      apply_reset(op.qubits, 0);
      break;
    case Operations::OpType::matrix:
      apply_matrix(op.qubits, op.mats[0]);
      break;
    case Operations::OpType::kraus:
      apply_kraus(op.qubits, op.mats);
      break;
    case Operations::OpType::gate:
      apply_gate(op);
      break;
    default: {
      std::stringstream msg;
      msg << "QubitVector::State::invalid instruction \'" << op.name << "\'.";
      throw std::invalid_argument(msg.str());
    }
  }
}


template <class statevec_t>
void State<statevec_t>::apply_gate(const Operations::Op &op) {
  // Look for gate name in gateset
  auto it = gateset.find(op.name);
  if (it == gateset.end()) {
    std::stringstream msg;
    msg << "QubitVectorState::invalid gate instruction \'" << op.name << "\'.";
    throw std::invalid_argument(msg.str());
  }
  Gates g = it -> second;
  switch (g) {
    case Gates::u3:
      apply_gate_u3(op.qubits[0],
                    std::real(op.params[0]),
                    std::real(op.params[1]),
                    std::real(op.params[2]));
      break;
    case Gates::u2:
      apply_gate_u3(op.qubits[0],
                    M_PI / 2.,
                    std::real(op.params[0]),
                    std::real(op.params[1]));
      break;
    case Gates::u1:
      apply_gate_phase(op.qubits[0], std::exp(complex_t(0., 1.) * op.params[0]));
      break;
    case Gates::cx:
      BaseState::data_.apply_cnot(op.qubits[0], op.qubits[1]);
      break;
    case Gates::cz:
      BaseState::data_.apply_cz(op.qubits[0], op.qubits[1]);
      break;
    case Gates::u0: // u0 = id in ideal State
      break;
    case Gates::id:
      break;
    case Gates::x:
      BaseState::data_.apply_x(op.qubits[0]);
      break;
    case Gates::y:
      BaseState::data_.apply_y(op.qubits[0]);
      break;
    case Gates::z:
      BaseState::data_.apply_z(op.qubits[0]);
      break;
    case Gates::h:
      apply_gate_u3(op.qubits[0], M_PI / 2., 0., M_PI);
      break;
    case Gates::s:
      apply_gate_phase(op.qubits[0], complex_t(0., 1.));
      break;
    case Gates::sdg:
      apply_gate_phase(op.qubits[0], complex_t(0., -1.));
      break;
    case Gates::t: {
      const double isqrt2{1. / std::sqrt(2)};
      apply_gate_phase(op.qubits[0], complex_t(isqrt2, isqrt2));
    } break;
    case Gates::tdg: {
      const double isqrt2{1. / std::sqrt(2)};
      apply_gate_phase(op.qubits[0], complex_t(isqrt2, -isqrt2));
    } break;
    case Gates::swap: {
      BaseState::data_.apply_swap(op.qubits[0], op.qubits[1]);
    } break;
    case Gates::rzz: {
      const auto dmat = rzz_diagonal_matrix(std::real(op.params[0]));
      BaseState::data_.apply_matrix(reg_t({op.qubits[0], op.qubits[1]}), dmat);
    } break;
    case Gates::ccx:
      BaseState::data_.apply_toffoli(op.qubits[0], op.qubits[1], op.qubits[2]);
      break;
    default: {
      // We shouldn't reach here unless there is a bug in gateset
      throw std::invalid_argument("QubitVector::State::invalid gate instruction \'" + op.name + "\'.");
    }
  }
}


//============================================================================
// Implementation: Matrix multiplication
//============================================================================

template <class statevec_t>
void State<statevec_t>::apply_matrix(const reg_t &qubits, const cmatrix_t &mat) {
  if (qubits.empty() == false && mat.size() > 0) {
    apply_matrix(qubits, Utils::vectorize_matrix(mat));
  }
}

template <class statevec_t>
void State<statevec_t>::apply_matrix(const reg_t &qubits, const cvector_t &vmat) {
  // Check if diagonal matrix
  if (vmat.size() == 1ULL << qubits.size()) {
    BaseState::data_.apply_diagonal_matrix(qubits, vmat);
  } else {
    BaseState::data_.apply_matrix(qubits, vmat);
  }
}

template <class statevec_t>
void State<statevec_t>::apply_gate_u3(uint_t qubit, double theta, double phi, double lambda) {
  apply_matrix(reg_t({qubit}), Utils::Matrix::U3(theta, phi, lambda));
}

template <class statevec_t>
void State<statevec_t>::apply_gate_phase(uint_t qubit, complex_t phase) {
  cvector_t diag = {{1., phase}};
  apply_matrix(reg_t({qubit}), diag);
}

template <class statevec_t>
cvector_t State<statevec_t>::rzz_diagonal_matrix(double lambda) {
  const complex_t one(1.0, 0);
  const complex_t phase = exp(complex_t(0, lambda));
  return cvector_t({one, phase, phase, one});
}


//============================================================================
// Implementation: Reset and Measurement Sampling
//============================================================================
template <class statevec_t>
void State<statevec_t>::apply_reset(const reg_t &qubits, const uint_t reset_state) {

  // Simulate unobserved measurement
  const auto meas = sample_measure_with_prob(qubits);
  // Apply update tp reset state
  measure_reset_update(qubits, reset_state, meas.first, meas.second);
}

template <class statevec_t>
std::pair<uint_t, double> State<statevec_t>::sample_measure_with_prob(const reg_t &qubits) {
  rvector_t probs = measure_probs(qubits);
  // Randomly pick outcome and return pair
  uint_t outcome = BaseState::rng_.rand_int(probs);
  return std::make_pair(outcome, probs[outcome]);
}

template <class statevec_t>
void State<statevec_t>::measure_reset_update(const std::vector<uint_t> &qubits,
                                 const uint_t final_state,
                                 const uint_t meas_state,
                                 const double meas_prob) {
  // Update a state vector based on an outcome pair [m, p] from 
  // sample_measure_with_prob function, and a desired post-measurement final_state
  
  // Single-qubit case
  if (qubits.size() == 1) {
    // Diagonal matrix for projecting and renormalizing to measurement outcome
    cvector_t mdiag(2, 0.);
    mdiag[meas_state] = 1. / std::sqrt(meas_prob);
    apply_matrix(qubits, mdiag);

    // If it doesn't agree with the reset state update
    if (final_state != meas_state) {
      BaseState::data_.apply_x(qubits[0]);
    }
  } 
  // Multi qubit case
  else {
    // Diagonal matrix for projecting and renormalizing to measurement outcome
    const size_t dim = 1ULL << qubits.size();
    cvector_t mdiag(dim, 0.);
    mdiag[meas_state] = 1. / std::sqrt(meas_prob);
    apply_matrix(qubits, mdiag);

    // If it doesn't agree with the reset state update
    // This function could be optimized as a permutation update
    if (final_state != meas_state) {
      // build vectorized permutation matrix  
      cvector_t perm(dim * dim, 0.); 
      perm[final_state * dim + meas_state] = 1.;
      perm[meas_state * dim + final_state] = 1.;
      for (size_t j=0; j < dim; j++) {
        if (j != final_state && j != meas_state)
          perm[j * dim + j] = 1.;
      }
      // apply permutation to swap state
      apply_matrix(qubits, perm);
    }
  }
}


//============================================================================
// Implementation: Kraus Noise
//============================================================================
template <class statevec_t>
void State<statevec_t>::apply_kraus(const reg_t &qubits,
                                 const std::vector<cmatrix_t> &kmats) {
  
  // Check edge case for empty Kraus set (this shouldn't happen)
  if (kmats.empty())
    return; // end function early


  // Choose a real in [0, 1) to choose the applied kraus operator once
  // the accumulated probability is greater than r.
  // We know that the Kraus noise must be normalized
  // So we only compute probabilities for the first N-1 kraus operators
  // and infer the probability of the last one from 1 - sum of the previous

  double r = BaseState::rng_.rand(0., 1.);
  double accum = 0.;
  bool complete = false;

  // Loop through N-1 kraus operators
  for (size_t j=0; j < kmats.size() - 1; j++) {
    
    // Calculate probability
    cvector_t vmat = Utils::vectorize_matrix(kmats[j]);
    double p = BaseState::data_.norm(qubits, vmat);
    accum += p;
    
    // check if we need to apply this operator
    if (accum > r) {
      // rescale vmat so projection is normalized
      Utils::scalar_multiply_inplace(vmat, 1 / std::sqrt(p));
      // apply Kraus projection operator
      apply_matrix(qubits, vmat);
      complete = true;
      break;
    }
  }

  // check if we haven't applied a kraus operator yet
  if (complete == false) {
    // Compute probability from accumulated
    complex_t renorm = 1 / std::sqrt(1. - accum);
    apply_matrix(qubits, Utils::vectorize_matrix(renorm * kmats.back()));
  }
}

//------------------------------------------------------------------------------
} // end namespace QubitVector
} // end namespace AER
//------------------------------------------------------------------------------
#endif
