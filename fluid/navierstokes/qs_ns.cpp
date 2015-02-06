/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t  -*- vim:set fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4

   This file is part of the Feel++ library

   Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   Date     : Tue Feb 25 12:13:15 2014

   Copyright (C) 2014-2015 Feel++ Consortium

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <feel/feel.hpp>

int main(int argc, char**argv )
{
    //! [marker1]
    using namespace Feel;
	po::options_description qsnsoptions( "Quickstart Navier-Stokes options" );
	qsnsoptions.add_options()
		( "mu", po::value<double>()->default_value( 1.0 ), "coeff" )
		;
	Environment env( _argc=argc, _argv=argv,
                     _desc=qsnsoptions,
                     _about=about(_name="qs_ns",
                                  _author="Feel++ Consortium",
                                  _email="feelpp-devel@feelpp.org"));
    constexpr int dim = FEELPP_DIM;
    constexpr int p_order = FEELPP_ORDER;
    tic();
    auto mesh = loadMesh( new Mesh<Simplex<dim>> );
    CHECK( mesh->hasMarkers( {"wall","inlet"} ) ) << "Mesh markers wall or inlet are not set properly in "  << soption("gmsh.filename");
    toc("mesh");tic();
    // Taylor Hood P_N+1 velocity P_N  pressure space (N==p_order)
    auto Vh = THch<p_order>( mesh );
    auto U = Vh->element();
    auto V = Vh->element();
    auto u = U.element<0>();
    auto v = V.element<0>();
    auto p = U.element<1>();
    auto q = V.element<1>();
    if ( Environment::isMasterRank() )
    {
        std::cout << " - number of global dof : " << Vh->nDof() << "\n";
        std::cout << " - number of local dof : " << Vh->nLocalDof() << "\n";
        std::cout << " - number of velocity dof : " << u.size() << "\n";
        std::cout << " - number of pressure dof : " << p.size() << "\n";
    }
    toc("space");tic();

    auto deft = gradt( u );
    auto def = grad( v );
    double mu = doption(_name="mu");

    auto mybdf = bdf( _space=Vh, _name="mybdf" );

    auto ft = form1( _test=Vh );

    auto a = form2( _trial=Vh, _test=Vh), at = form2( _trial=Vh, _test=Vh);

    a = integrate( _range=elements( mesh ), _expr=mu*inner( deft, grad(v) ) + mybdf->polyDerivCoefficient(0)*trans(idt(u))*id(u) );
    a +=integrate( _range=elements( mesh ), _expr=-div( v )*idt( p ) - divt( u )*id( q ) );
    auto e = exporter( _mesh=mesh );
    auto w = Vh->functionSpace<0>()->element( curlv(u), "w" );

    BoundaryConditions bcs;
    map_vector_field<dim,1,2> dirichlet_conditions { bcs.getVectorFields<dim> ( "velocity", "Dirichlet" ) };
    
    toc("bdf, forms,...");

    for ( mybdf->start();  mybdf->isFinished() == false; mybdf->next(U) )
    {
        if ( Environment::isMasterRank() )
        {
            std::cout << "------------------------------------------------------------\n";
            std::cout << "Time " << mybdf->time() << "s\n";
        }
        tic();
        tic();
        auto bdf_poly = mybdf->polyDeriv();
        auto rhsu =  bdf_poly.element<0>();
        auto extrap = mybdf->poly();
        auto extrapu = extrap.element<0>();
        // add BDF term to the right hand side from previous time steps
        ft = integrate( _range=elements(mesh), _expr=(trans(idv(rhsu))*id(u) ) );
        toc("update rhs");tic();

        at.zero();
        at += a;
        at += integrate( _range=elements( mesh ), _expr= trans(gradt(u)*idv(extrapu))*id(v) );
        for( auto const& condition : dirichlet_conditions )
        {
            at+=on(_range=markedfaces(mesh,marker(condition)), _rhs=ft, _element=u,
                   _expr=expression(condition));
        }
        toc("update lhs");tic();
        
        at.solve(_rhs=ft,_solution=U);
        toc("solve");tic();

        w.on( _range=elements(mesh), _expr=curlv(u) );
        e->step(mybdf->time())->add( "u", u );
        e->step(mybdf->time())->add( "w", w );
        e->step(mybdf->time())->add( "p", p );
        auto mean_p = mean( _range=elements(mesh), _expr=idv(p) )( 0, 0 );
        e->step(mybdf->time())->addScalar( "mean_p", mean_p );
        e->save();
        toc("export");
        toc("time step");


    }
    //! [marker1]



    return 0;
}
