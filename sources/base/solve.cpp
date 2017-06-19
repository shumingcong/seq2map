#include <boost/thread.hpp>
#include <seq2map/solve.hpp>

using namespace seq2map;

size_t LeastSquaresProblem::GetHardwareConcurrency()
{
    return boost::thread::hardware_concurrency();
}

VectorisableD::Vec LeastSquaresProblem::operator() (const VectorisableD& vec) const
{
    VectorisableD::Vec v;

    if (!vec.Store(v))
    {
        E_ERROR << "vectorisation failed";
        return VectorisableD::Vec();
    }

    return (*this)(v);
}

cv::Mat LeastSquaresProblem::ComputeJacobian(const VectorisableD::Vec& x, VectorisableD::Vec& y) const
{
    cv::Mat J = cv::Mat::zeros(static_cast<int>(m_conds), static_cast<int>(m_varIdx.size()), CV_64F);
    bool masking = !m_jacobianPattern.empty();

    boost::thread_group threads;

    y = y.empty() ? (*this)(x) : y;

    // make evaluation slices for multi-threaded numerical differentiation
    std::vector<JacobianSlices> slices(m_diffThreads);
    size_t i = 0;

    for (Indices::const_iterator var = m_varIdx.begin(); var != m_varIdx.end(); var++)
    {
        JacobianSlice slice;
        size_t threadIdx = i % m_diffThreads;

        slice.x    = x;
        slice.y    = y;
        slice.var  = *var;
        slice.col  = J.col(static_cast<int>(i));
        slice.mask = masking ? m_jacobianPattern.col(static_cast<int>(*var)) : cv::Mat();

        slices[threadIdx].push_back(slice);

        i++;
    }

    // lunch the threads
    for (size_t k = 0; k < slices.size(); k++)
    {
        //threads.push_back(boost::thread(
        threads.add_thread(new boost::thread(
            LeastSquaresProblem::DiffThread,
            (const LeastSquaresProblem*) this,
            slices[k])
        );
    }

    // wait for completions
    threads.join_all();

    return J;
}

void LeastSquaresProblem::DiffThread(const LeastSquaresProblem* lsq, JacobianSlices& slices)
{
    BOOST_FOREACH (JacobianSlice& slice, slices)
    {
        cv::Mat x = cv::Mat(slice.x).clone();
        cv::Mat y = cv::Mat(slice.y).clone();
        double dx = lsq->m_diffStep;

        x.at<double>(static_cast<int>(slice.var)) += dx;
        cv::Mat dy = cv::Mat((*lsq)(x)) - y;

        // Jk = (f(x+dx) - f(x)) / dx
        cv::divide(dy, dx, slice.col);
    }
}

bool LeastSquaresProblem::SetActiveVars(const Indices& varIdx)
{
    BOOST_FOREACH(size_t var, varIdx)
    {
        if (var >= m_vars) return false;
    }

    m_varIdx = varIdx;

    return true;
}

cv::Mat LeastSquaresProblem::ApplyUpdate(const VectorisableD::Vec& x0, const VectorisableD::Vec& delta)
{
    assert(delta.size() <= x0.size());
    VectorisableD::Vec x = x0;
    size_t i = 0;
    
    BOOST_FOREACH(size_t var, m_varIdx)
    {
        x[var] += delta[i++];
    }

    return cv::Mat(x, true);
}

bool LevenbergMarquardtAlgorithm::Solve(LeastSquaresProblem& f, const VectorisableD::Vec& x0)
{
    assert(m_eta > 1.0f);

    double lambda = m_lambda;
    bool converged = false;
    std::vector<double> derr;

    VectorisableD::Vec x_best = x0;
    VectorisableD::Vec y_best = f(x0);
    double e_best = rms(cv::Mat(y_best));

    size_t updates = 0; // iteration number
    if (m_verbose)
    {
        E_INFO << std::setw(80) << std::setfill('=') << "";
        E_INFO << std::setw(6) << std::right << "Update" << std::setw(12) << std::right << "RMSE" << std::setw(16) << std::right << "lambda" << std::setw(16) << std::right << "Rel. Step Size" << std::setw(16) << std::right << "Rel. Error Drop";
        E_INFO << std::setw(80) << std::setfill('=') << "";
        E_INFO << std::setw(6) << std::right << updates << std::setw(12) << std::right << e_best << std::setw(16) << std::right << lambda;
    }

    try
    {
        while (!converged)
        {
            cv::Mat J = f.ComputeJacobian(x_best, y_best);

            cv::Mat H = J.t() * J; // Hessian matrix
            cv::Mat D = J.t() * cv::Mat(y_best); // error gradient

            lambda = lambda < 0 ? cv::mean(H.diag())[0] : lambda;

            bool better = false;
            size_t trials = 0;
            double derrRatio, stepRatio;

            while (!better && !converged)
            {
                // augmented normal equations
                cv::Mat A = H + lambda * cv::Mat::diag(H.diag()); // or A = N + lambda*eye(d);
                cv::Mat x_delta = A.inv() * -D; // x_delta =  A \ -D;

                VectorisableD::Vec x_try = f.ApplyUpdate(x_best, x_delta); // = cv::Mat(cv::Mat(x_best) + x_delta);
                VectorisableD::Vec y_try = f(x_try);

                double e_try = rms(cv::Mat(y_try));
                double de = e_best - e_try;

                better = de > 0;
                trials++;

                if (better) // accept the update
                {
                    lambda /= m_eta;

                    x_best = x_try;
                    y_best = y_try;
                    e_best = e_try;

                    derr.push_back(de);
                    updates++;
                }
                else // reject the update
                {
                    lambda *= m_eta;
                }

                // convergence control
                derrRatio = derr.size() > 1 ? (derr[derr.size() - 1] / derr[derr.size() - 2]) : 1.0f;
                stepRatio = norm(x_delta) / norm(cv::Mat(x_best));

                converged |= (updates >= m_term.maxCount); // # iterations check
                converged |= (updates > 1) && (derrRatio < m_term.epsilon); // error differential check
                converged |= (updates > 1) && (stepRatio < m_term.epsilon); // step ratio check
                converged |= (!better && (lambda == 0 || trials >= m_term.maxCount));

                cv::Mat icv = H.diag();

                bool ill = false;

                for (size_t d = 0; d < icv.total(); d++)
                {
                    if (icv.at<double>(d) == 0)
                    {
                        E_WARNING << "change of parameter " << d << " not responsive";
                        ill = true;
                    }
                }

                ill |= !isfinite(norm(x_delta));

                if (ill)
                {
                    E_ERROR << "problem ill-posed";
                    return false;
                }
            }

            if (m_verbose)
            {
                E_INFO << std::setw(6) << std::right << updates << std::setw(12) << std::right << e_best << std::setw(16) << std::right << lambda << std::setw(16) << std::right << stepRatio << std::setw(16) << std::right << derrRatio;
            }

            //if (updates == 1 && mat2raw(J, "jacob.raw"))
            //{
            //    E_INFO << "Jacobian (" << size2string(J.size()) << ") written";
            //}
        }
    }
    catch (std::exception& ex)
    {
        E_ERROR << "exception caught in optimisation loop";
        E_ERROR << ex.what();
        return false;
    }

    if (!f.SetSolution(x_best))
    {
        E_ERROR << "error setting solution";
        E_ERROR << mat2string(cv::Mat(x_best), "x_best");

        return false;
    }

    return true;
}
