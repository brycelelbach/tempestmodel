///////////////////////////////////////////////////////////////////////////////
///
///	\file    BaroclinicWaveUMJSTest.cpp
///	\author  Paul Ullrich
///	\version June 23, 2013
///
///	<remarks>
///		Copyright 2000-2010 Paul Ullrich
///
///		This file is distributed as part of the Tempest source code package.
///		Permission is granted to use, copy, modify and distribute this
///		source code and its documentation under the terms of the GNU General
///		Public License.  This software is provided "as is" without express
///		or implied warranty.
///	</remarks>

#include "Defines.h"
#include "CommandLine.h"
#include "Announce.h"

#include "Model.h"
#include "TimestepSchemeARK4.h"
#include "HorizontalDynamicsFEM.h"
#include "VerticalDynamicsFEM.h"

#include "PhysicalConstants.h"
#include "TestCase.h"
#include "OutputManagerComposite.h"
#include "OutputManagerReference.h"
#include "OutputManagerChecksum.h"
#include "GridData4D.h"
#include "EquationSet.h"

#include "GridCSGLL.h"

#ifdef USE_PETSC
#include <petscsnes.h>
#else
#include "mpi.h"
#endif

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		Ullrich, Melvin, Jablonowski and Staniforth (2013) Baroclinic wave test
///	</summary>
class BaroclinicWaveUMJSTest : public TestCase {

public:
	///	<summary>
	///		Model scaling parameter
	///	</summary>
	static const double ParamEarthRadiusScaling = 1.0;

	///	<summary>
	///		Model height limit
	///	</summary>
	static const double ParamHeightLimit = 30000.0;

	///	<summary>
	///		Equatorial surface temperature (K)
	///	</summary>
	static const double ParamT0E = 310.0;

	///	<summary>
	///		Polar surface temperature (K)
	///	</summary>
	static const double ParamT0P = 240.0;

	///	<summary>
	///		Half-width parameter
	///	</summary>
	static const double ParamB = 2.0;

	///	<summary>
	///		Jet parameter
	///	</summary>
	static const double ParamK = 3.0;

	///	<summary>
	///		Temperature lapse rate
	///	</summary>
	static const double ParamLapseRate = 0.005;

	///	<summary>
	///		Stream function perturbation wind parameter
	///	</summary>
	static const double ParamU0 = 0.5;

	///	<summary>
	///		Stream function perturbation radius (Earth radii)
	///	</summary>
	static const double ParamPertR = 1.0 / 6.0;

	///	<summary>
	///		Expontential zonal wind perturbation
	///	</summary>
	static const double ParamUp = 1.0;

	///	<summary>
	///		Exponential perturbation radius (Earth radii)
	///	</summary>
	static const double ParamPertExpR = 0.1;

	///	<summary>
	///		Perturbation longitude center (radians)
	///	</summary>
	static const double ParamPertLon = M_PI / 9.0;

	///	<summary>
	///		Perturbation latitude center (radians)
	///	</summary>
	static const double ParamPertLat = 2.0 * M_PI / 9.0;

	///	<summary>
	///		Height cap of the perturbation (polynomial tapering)
	///	</summary>
	static const double ParamPertZ = 15000.0;

protected:
	///	<summary>
	///		Alpha parameter.
	///	</summary>
	double m_dAlpha;

	///	<summary>
	///		Deep atmosphere flag.
	///	</summary>
	bool m_fDeepAtmosphere;

	///	<summary>
	///		Whether to use an auxilliary tracer field.
	///	</summary>
	bool m_fTracerOn;

	///	<summary>
	///		Model height cap.
	///	</summary>
	double m_dZtop;

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	BaroclinicWaveUMJSTest(
		double dAlpha,
		bool fDeepAtmosphere,
		bool fTracerOn,
		double dZtop
	) :
		m_dAlpha(dAlpha),
		m_fDeepAtmosphere(fDeepAtmosphere),
		m_fTracerOn(fTracerOn),
		m_dZtop(dZtop)
	{ }

public:
	///	<summary>
	///		Number of tracers used in this test.
	///	</summary>
	virtual int GetTracerCount() const {
		return (m_fTracerOn)?(1):(0);
	}

	///	<summary>
	///		Get the altitude of the model cap.
	///	</summary>
	virtual double GetZtop() const {
		return m_dZtop;
	}

	///	<summary>
	///		Flag indicating that a reference state is available.
	///	</summary>
	virtual bool HasReferenceState() const {
		return false;
	}

	///	<summary>
	///		Obtain test case specific physical constants.
	///	</summary>
	virtual void EvaluatePhysicalConstants(
		PhysicalConstants & phys
	) const {
		// Set the alpha parameter
		phys.SetAlpha(m_dAlpha);
	}

	///	<summary>
	///		Evaluate the topography at the given point.
	///	</summary>
	virtual double EvaluateTopography(
		const PhysicalConstants & phys,
		double dLon,
		double dLat
	) const {
		return (0.0);
	}

	///	<summary>
	///		Evaluate the reference state at the given point.
	///	</summary>
	virtual void EvaluateReferenceState(
		const PhysicalConstants & phys,
		double dZ,
		double dLon,
		double dLat,
		double * dState
	) const {

		// Radius
		double dR = dZ + phys.GetEarthRadius();

		// Calculate parameters
		double dT0 = 0.5 * (ParamT0E + ParamT0P);

		double dConstA = 1.0 / ParamLapseRate;

		double dConstB = (dT0 - ParamT0P) / (dT0 * ParamT0P);

		double dConstC = 0.5 * (ParamK + 2.0)
			* (ParamT0E - ParamT0P) / (ParamT0E * ParamT0P);

		double dConstH = phys.GetR() * dT0 / phys.GetG();

		// Computed quantities
		double dScaledZ = dZ / (ParamB * dConstH);

		// Calculate tau values
		double dTau1 =
			dConstA * ParamLapseRate / dT0
				* exp(ParamLapseRate / dT0 * dZ)
			+ dConstB
				* (1.0 - 2.0 * dScaledZ * dScaledZ)
				* exp(- dScaledZ * dScaledZ);

		double dTau2 =
			dConstC * (1.0 - 2.0 * dScaledZ * dScaledZ)
				* exp(- dScaledZ * dScaledZ);

		double dIntTau1 =
			dConstA * (exp(ParamLapseRate / dT0 * dZ) - 1.0)
			+ dConstB * dZ * exp(- dScaledZ * dScaledZ);

		double dIntTau2 =
			dConstC * dZ * exp(- dScaledZ * dScaledZ);

		// Calculate utility terms
		double dRRatio;
		if (m_fDeepAtmosphere) {
			dRRatio = dR / phys.GetEarthRadius();
		} else {
			dRRatio = 1.0;
		}

		double dInteriorTerm = pow(dRRatio * cos(dLat), ParamK)
			- ParamK / (ParamK + 2.0) * pow(dRRatio * cos(dLat), ParamK + 2.0);

		// Calculate temperature
		double dTemperature = 1.0
			/ (dRRatio * dRRatio)
			/ (dTau1 - dTau2 * dInteriorTerm);

		// Calculate hydrostatic pressure
		double dPressure = phys.GetP0() * exp(
			- phys.GetG() / phys.GetR()
				* (dIntTau1 - dIntTau2 * dInteriorTerm));
/*
		// Calculate pressure derivative
		double dDrPressure;
		if (m_fDeepAtmosphere) {
			dDrPressure =
				dPressure * phys.GetG() / phys.GetR() * (
					- dTau1 + dTau2 * dInteriorTerm
					+ dIntTau2 * ParamK / dR
						* (pow(dRRatio * cos(dLat), ParamK)
							- pow(dRRatio * cos(dLat), ParamK + 2.0)));

		} else {
			dDrPressure =
				dPressure * phys.GetG() / phys.GetR() * (
					- dTau1 + dTau2 * dInteriorTerm);
		}

		// Calculate hydrostatic density
		double dHydroRho = - dRRatio * dRRatio / phys.GetG() * dDrPressure;
*/
		// Calculate exact density
		double dRho = dPressure / (phys.GetR() * dTemperature);

		// Store the state
		dState[0] = 0.0;
		dState[1] = 0.0;
		dState[2] = phys.RhoThetaFromPressure(dPressure) / dRho;
		dState[3] = 0.0;
		dState[4] = dRho;
	}

	///	<summary>
	///		Evaluate the state vector at the given point.
	///	</summary>
	virtual void EvaluatePointwiseState(
		const PhysicalConstants & phys,
		double dTime,
		double dZ,
		double dLon,
		double dLat,
		double * dState,
		double * dTracer
	) const {

		// Radius
		double dR = dZ + phys.GetEarthRadius();

		// Calculate parameters
		double dT0 = 0.5 * (ParamT0E + ParamT0P);

		double dConstA = 1.0 / ParamLapseRate;

		double dConstB = (dT0 - ParamT0P) / (dT0 * ParamT0P);

		double dConstC = 0.5 * (ParamK + 2.0)
			* (ParamT0E - ParamT0P) / (ParamT0E * ParamT0P);

		double dConstH = phys.GetR() * dT0 / phys.GetG();

		// Computed quantities
		double dScaledZ = dZ / (ParamB * dConstH);

		// Calculate tau values
		double dTau1 =
			dConstA * ParamLapseRate / dT0
				* exp(ParamLapseRate / dT0 * dZ)
			+ dConstB
				* (1.0 - 2.0 * dScaledZ * dScaledZ)
				* exp(- dScaledZ * dScaledZ);

		double dTau2 =
			dConstC * (1.0 - 2.0 * dScaledZ * dScaledZ)
				* exp(- dScaledZ * dScaledZ);

		double dIntTau1 =
			dConstA * (exp(ParamLapseRate / dT0 * dZ) - 1.0)
			+ dConstB * dZ * exp(- dScaledZ * dScaledZ);

		double dIntTau2 =
			dConstC * dZ * exp(- dScaledZ * dScaledZ);

		// Calculate utility terms
		double dRRatio;
		if (m_fDeepAtmosphere) {
			dRRatio = dR / phys.GetEarthRadius();
		} else {
			dRRatio = 1.0;
		}

		double dInteriorTerm = pow(dRRatio * cos(dLat), ParamK)
			- ParamK / (ParamK + 2.0) * pow(dRRatio * cos(dLat), ParamK + 2.0);

		// Calculate temperature
		double dTemperature = 1.0
			/ (dRRatio * dRRatio)
			/ (dTau1 - dTau2 * dInteriorTerm);

		// Calculate hydrostatic pressure
		double dPressure = phys.GetP0() * exp(
			- phys.GetG() / phys.GetR()
				* (dIntTau1 - dIntTau2 * dInteriorTerm));

		// Calculate hydrostatic density
		double dRho = dPressure / (phys.GetR() * dTemperature);

		// Velocity field
		double dInteriorTermU =
			  pow(dRRatio * cos(dLat), ParamK - 1.0)
			- pow(dRRatio * cos(dLat), ParamK + 1.0);

		double dBigU = phys.GetG() / phys.GetEarthRadius() * ParamK
			* dIntTau2 * dInteriorTermU * dTemperature;

		double dRCosLat;
		if (m_fDeepAtmosphere) {
			dRCosLat = dR * cos(dLat);
		} else {
			dRCosLat = phys.GetEarthRadius() * cos(dLat);
		}

		double dOmegaRCosLat = phys.GetOmega() * dRCosLat;

		if (dOmegaRCosLat * dOmegaRCosLat + dRCosLat * dBigU < 0.0) {
			_EXCEPTIONT("Negative discriminant detected.");
		}

		double dUlon = - dOmegaRCosLat +
			sqrt(dOmegaRCosLat * dOmegaRCosLat + dRCosLat * dBigU);

		// Store the state
		dState[0] = dUlon;
		dState[1] = 0.0;
		dState[2] = phys.RhoThetaFromPressure(dPressure) / dRho;
		dState[3] = 0.0;
		dState[4] = dRho;

	}
};

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {

#ifdef USE_PETSC
	// Initialize PetSc
	PetscInitialize(&argc,&argv, NULL, NULL);
#else
	// Initialize MPI
	MPI_Init(&argc, &argv);
#endif

try {
	// Output directory
	std::string strOutputDir;

	// Output file prefix
	std::string strOutputPrefix;

	// Resolution
	int nResolution;

	// Vertical resolution
	int nLevels;

	// Order
	int nHorizontalOrder;

	// Vertical Order
	int nVerticalOrder;

	// Model height cap
	double dZtop;

	// Grid rotation angle
	double dAlpha;

	// Include tracer field
	bool fTracersOn;

	// Deep atmosphere flag
	bool fDeepAtmosphere;

	// Output time
	double dOutputDeltaT;

	// Model parameters
	ModelParameters params;

	// Parse the command line
	BeginCommandLine()
		CommandLineString(strOutputDir, "output_dir", "out");
		CommandLineString(strOutputPrefix, "output_prefix", "out");
		CommandLineInt(nResolution, "resolution", 20);
		CommandLineInt(nLevels, "levels", 10);
		CommandLineInt(nHorizontalOrder, "order", 4);
		CommandLineInt(nVerticalOrder, "vertorder", 1);
		CommandLineDouble(dZtop, "ztop", 10000.0);
		CommandLineDouble(dAlpha, "alpha", 0.0);
		CommandLineBool(fTracersOn, "with_tracer");
		CommandLineBool(fDeepAtmosphere, "deep_atmosphere");
		CommandLineDouble(params.m_dDeltaT, "dt", 200.0);
		CommandLineDouble(params.m_dEndTime, "endtime", 200.0);
		CommandLineDouble(dOutputDeltaT, "outputtime", 86400.0);

		ParseCommandLine(argc, argv);
	EndCommandLine(argv)

	AnnounceBanner("INITIALIZATION");

	// Construct a model
	AnnounceStartBlock("Creating model");
	Model model(EquationSet::PrimitiveNonhydrostaticEquations);
	AnnounceEndBlock("Done");

	// Set the parameters for the model
	AnnounceStartBlock("Initializing parameters");
	model.SetParameters(&params);
	AnnounceEndBlock("Done");

	// Set the timestep scheme
	TimestepSchemeARK4 timestep(model);
	AnnounceStartBlock("Initializing timestep scheme");
	model.SetTimestepScheme(&timestep);
	AnnounceEndBlock("Done");

	// Set the horizontal dynamics
	HorizontalDynamicsFEM hdyn(model, nHorizontalOrder);
	AnnounceStartBlock("Initializing horizontal dynamics");
	model.SetHorizontalDynamics(&hdyn);
	AnnounceEndBlock("Done");

	// Set the vertical dynamics
	VerticalDynamicsFEM vdyn(model, nHorizontalOrder, nVerticalOrder);
	AnnounceStartBlock("Initializing vertical dynamics");
	model.SetVerticalDynamics(&vdyn);
	AnnounceEndBlock("Done");

	// Create a new grid for the model
	AnnounceStartBlock("Initializing grid");
	GridCSGLL grid(
		model,
		nResolution,
		4,
		nHorizontalOrder,
		nVerticalOrder,
		nLevels);

	model.SetGrid(&grid);
	AnnounceEndBlock("Done");

	// Set the test case for the model
	BaroclinicWaveUMJSTest test(dAlpha, fDeepAtmosphere, fTracersOn, dZtop);
	AnnounceStartBlock("Initializing data");
	model.SetTestCase(&test);
	AnnounceEndBlock("Done");

	// Set the reference output manager for the model
	AnnounceStartBlock("Creating reference output manager");
	OutputManagerReference outmanRef(
		grid, dOutputDeltaT, strOutputDir, strOutputPrefix,
		360, 180, false, false);
	model.AttachOutputManager(&outmanRef);

	outmanRef.InitializeNcOutput("ref.nc");
	AnnounceEndBlock("Done");
/*
	// Set the composite output manager for the model
	AnnounceStartBlock("Creating composite output manager");
	OutputManagerComposite outmanComp(
		grid, dOutputDeltaT, strOutputDir, strOutputPrefix);
	model.AttachOutputManager(&outmanComp);

	outmanComp.InitializeNcOutput("comp.nc");
	AnnounceEndBlock("Done");
*/
	// Set the checksum output manager for the model
	AnnounceStartBlock("Creating checksum output manager");
	OutputManagerChecksum outmanChecksum(grid, dOutputDeltaT);
	model.AttachOutputManager(&outmanChecksum);
	AnnounceEndBlock("Done");

	// Begin execution
	AnnounceBanner("SIMULATION");
	model.Go();

	// Execution complete
	//AnnounceBanner("Execution completed successfully");

	// Compute error norms
	AnnounceBanner("RESULTS");
	model.ComputeErrorNorms();
	AnnounceBanner();

} catch(Exception & e) {
	std::cout << e.ToString() << std::endl;
}

#ifdef USE_PETSC
	// Finalize PetSc
	PetscFinalize();
#else
	// Finalize MPI
	MPI_Finalize();
#endif
}

///////////////////////////////////////////////////////////////////////////////
