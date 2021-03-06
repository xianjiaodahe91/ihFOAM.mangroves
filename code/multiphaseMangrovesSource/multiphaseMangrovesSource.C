/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2017 OpenCFD Ltd.
     \\/     M anipulation  | Copyright (C) 2017 IH-Cantabria
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "multiphaseMangrovesSource.H"
#include "mathematicalConstants.H"
#include "fvMesh.H"
#include "fvMatrices.H"
#include "fvmDdt.H"
#include "fvcGrad.H"
#include "fvmDiv.H"
#include "fvmLaplacian.H"
#include "fvmSup.H"
#include "surfaceInterpolate.H"
#include "surfaceFields.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{
    defineTypeNameAndDebug(multiphaseMangrovesSource, 0);
    addToRunTimeSelectionTable
    (
        option,
        multiphaseMangrovesSource,
        dictionary
    );
}
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::fv::multiphaseMangrovesSource::multiphaseMangrovesSource
(
    const word& name,
    const word& modelType,
    const dictionary& dict,
    const fvMesh& mesh
)
:
    option(name, modelType, dict, mesh),
    aZone_(),
    NZone_(),
    CmZone_(),
    CdZone_()
{
    read(dict);
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::fv::multiphaseMangrovesSource::addSup
(
    const volScalarField& rho,
    fvMatrix<vector>& eqn,
    const label fieldi
)
{
    const volVectorField& U = eqn.psi();

    volScalarField DragFoceMangroves
    (
        IOobject
        (
            typeName + ":DragFoceMangroves",
            mesh_.time().timeName(),
            mesh_.time(),
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        mesh_,
        dimensionedScalar("DragFoceMangroves", dimDensity/dimTime, 0)
    );

    volScalarField InertiaFoceMangroves
    (
        IOobject
        (
            typeName + ":InertiaFoceMangroves",
            mesh_.time().timeName(),
            mesh_.time(),
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        mesh_,
        dimensionedScalar("InertiaFoceMangroves", dimless, 0)
    );

    const scalar pi = constant::mathematical::pi;

    forAll(zoneIDs_, i)
    {
        const labelList& zones = zoneIDs_[i];

        forAll(zones, j)
        {
            const label zonei = zones[j];
            const cellZone& cz = mesh_.cellZones()[zonei];

            forAll(cz, k)
            {
                const label celli = cz[k];
                const scalar a = aZone_[i];
                const scalar N = NZone_[i];
                const scalar Cm = CmZone_[i];
                const scalar Cd = CdZone_[i];

                const scalar rhoc = rho[celli];
                const vector& Uc = U[celli];

                DragFoceMangroves[celli] = 0.5 * rhoc * Cd * a * N * mag(Uc);
                InertiaFoceMangroves[celli] = rhoc * (Cm+1) * pi * a * a / 4.0;
            }
        }
    }  

    DragFoceMangroves.correctBoundaryConditions();
    InertiaFoceMangroves.correctBoundaryConditions();

    fvMatrix<vector> MangrovesEqn
    (
      - fvm::Sp(DragFoceMangroves, U)
      - InertiaFoceMangroves*fvm::ddt(rho, U)
    );

    // Contributions are added to RHS of momentum equation
    eqn += MangrovesEqn;
}


bool Foam::fv::multiphaseMangrovesSource::read(const dictionary& dict)
{
    if (option::read(dict))
    {
        if (coeffs_.found("UNames"))
        {
            coeffs_.lookup("UNames") >> fieldNames_;
        }
        else if (coeffs_.found("U"))
        {
            word UName(coeffs_.lookup("U"));
            fieldNames_ = wordList(1, UName);
        }
        else
        {
            fieldNames_ = wordList(1, "U");
        }

        applied_.setSize(fieldNames_.size(), false);

        // Create the Mangroves models - 1 per region
        const dictionary& regionsDict(coeffs_.subDict("regions"));
        const wordList regionNames(regionsDict.toc());
        aZone_.setSize(regionNames.size(), 1);
        NZone_.setSize(regionNames.size(), 1);
        CmZone_.setSize(regionNames.size(), 1);
        CdZone_.setSize(regionNames.size(), 1);
        zoneIDs_.setSize(regionNames.size());

        forAll(zoneIDs_, i)
        {
            const word& regionName = regionNames[i];
            const dictionary& modelDict = regionsDict.subDict(regionName);

            const word& zoneName = modelDict.lookup("cellZone");
            zoneIDs_[i] = mesh_.cellZones().findIndices(zoneName);
            if (zoneIDs_[i].empty())
            {
                FatalErrorInFunction
                    << "Unable to find cellZone " << zoneName << nl
                    << "Valid cellZones are:" << mesh_.cellZones().names()
                    << exit(FatalError);
            }

            modelDict.lookup("a") >> aZone_[i];
            modelDict.lookup("N") >> NZone_[i];
            modelDict.lookup("Cm") >> CmZone_[i];
            modelDict.lookup("Cd") >> CdZone_[i];
        }

        return true;
    }
    else
    {
        return false;
    }
}


// ************************************************************************* //
