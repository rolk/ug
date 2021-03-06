// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#include <stdlib.h>
#include <stdio.h>
#include <fstream.h>
#include <iostream.h>
#include <iomanip.h>
#include <math.h>
#include <new.h>
#include <ctype.h>
#include <string.h>

#include <template.hh>
#include <array.hh>
#include <table.hh>

#include <geom/geom2d.hh>
#include <geom/geom3d.hh>
#include <linalg/linalg.hh>


#include <meshing/meshtype.hh>
#include <meshing/global.hh>

#include <meshing/adfront2.hh>
#include <meshing/ruler2.hh>

#include <meshing/meshing2.hh>


extern void PlotSurface (const ROT3D & r, char key);
extern void SteepestDescent (Vector & x, double (*f)(const Vector & x, Vector & g));
extern void Drawowngeo(Element & ele,const ROT3D & r);




static ARRAY<Point3d> locpoints;
static ARRAY<Point2d> plainpoints;
static ARRAY<ILINE> loclines;
static int cntelem;
static int oldnl;
static int qualclass, surfind;
static const Meshing2 * meshthis;


Meshing2 :: Meshing2 (char * rulefilename)
{
  //  LoadRules ("triangle.rls");
  adfront = new ADFRONT2();
}

Meshing2 :: ~Meshing2 ()
{
  int i;
  if (adfront) delete adfront;
  adfront = NULL;
  for (i = 1; i <= rules.Size(); i++)
    delete rules[i];
  rules.SetSize (0);
}

void Meshing2 :: AddPoint (const Point3d & p, INDEX globind)
{
  adfront ->AddPoint (p, globind);
}

void Meshing2 :: AddBoundaryElement (INDEX i1, INDEX i2, int surfind)
{
  adfront ->AddLine (i1, i2, surfind);
}


void Meshing2 :: StartMesh ()
{
  int i;

  ruleused.SetSize (rules.Size());
  for (i = 1; i <= ruleused.Size(); i++)
    ruleused[i] = 0;
  cntelem = 0;
}

void Meshing2 :: EndMesh ()
{}

double Meshing2 :: CalcLocalH (const Point3d &  p, int /* surfind */,
                               double gh) const
{
  return gh;
}



static Vec3d ex, ey;
static Point3d globp1;

void Meshing2 :: DefineTransformation (INDEX /* surfind */, Point3d & p1, Point3d & p2)
{
  globp1 = p1;
  ex = p2 - p1;
  ex /= ex.Length();
  ey.X() = -ex.Y();
  ey.Y() =  ex.X();
  ey.Z() = 0;
}

void Meshing2 :: TransformToPlain (INDEX /* surfind */, const Point3d & locpoint,
                                   Point2d & plainpoint, double h)
{
  Vec3d p1p;

  p1p = locpoint - globp1;
  p1p /= h;
  plainpoint.X() = p1p * ex;
  plainpoint.Y() = p1p * ey;
}

void Meshing2 :: TransformFromPlain (INDEX /* surfind */, Point2d & plainpoint,
                                     Point3d & locpoint, double h)
{
  Vec3d p1p;

  p1p = plainpoint.X() * ex + plainpoint.Y() * ey;
  p1p *= h;
  locpoint = globp1 + p1p;
}

void Meshing2 :: GetNormalVector(INDEX /* surfind */, const Point3d & /* p */, Vec3d & n) const
{
  n.X() = 0;
  n.Y() = 0;
  n.Z() = 1;
}





void Meshing2 :: Mesh (double gh)
{
  ARRAY<INDEX> pindex, lindex;
  ARRAY<int> delpoints, dellines;
  ARRAY<Element> locelements;
  int i, j, oldnp,flag;
  char found;
  INDEX globind;
  static ARRAY<Point3d> locp;

  double h;
  Point2d bemp, bemp1, bemp2;

  testmode = 0;
  meshthis = this;

  StartMesh();

  adfront ->SetStartFront ();
  while (!adfront ->Empty())

  {

    qualclass =
      adfront ->GetLocals (locpoints, loclines, pindex, lindex,
                           surfind, 60 * gh, 1.0);
    oldnp = locpoints.Size();
    oldnl = loclines.Size();


    bemp1.X() = locpoints[loclines[1].I1()].X();
    bemp1.Y() = locpoints[loclines[1].I1()].Y();
    bemp2.X() = locpoints[loclines[1].I2()].X();
    bemp2.Y() = locpoints[loclines[1].I2()].Y();

    bemp = Center (bemp1, bemp2);
    bemp.Y() += 0.1 * (bemp2.X() - bemp1.X());
    bemp.X() -= 0.1 * (bemp2.Y() - bemp1.Y());

    h = CalcLocalH (Point3d (bemp.X(), bemp.Y(), 0), surfind, gh);

    DefineTransformation (surfind, locpoints[loclines[1].I1()],
                          locpoints[loclines[1].I2()]);

    plainpoints.SetSize (locpoints.Size());
    for (i = 1; i <= locpoints.Size(); i++)
      TransformToPlain (surfind, locpoints[i], plainpoints[i], h);

    cout << " locpoints and plainpoints" << endl;
    for (i = 1; i <= locpoints.Size(); i++)
    {
      cout << locpoints[i].X() << "  " <<  locpoints[i].Y() << "  " <<  locpoints[i].Z() << endl;
      cout << plainpoints[i].X() << "  " <<  plainpoints[i].Y() << endl;
    }


    for (i = 2; i <= loclines.Size(); i++)  // don't remove first line
    {
      if (plainpoints[loclines[i].I1()].X() > 1e6 ||
          plainpoints[loclines[i].I2()].X() > 1e6)
      {
        loclines.DeleteElement(i);
        lindex.DeleteElement(i);
        oldnl--;
        i--;
      }
    }


    found = ApplyRules (rules, plainpoints, loclines, locelements,
                        dellines, qualclass);

    flag = 1;
    if(found)
      locpoints.SetSize (plainpoints.Size());
    for (i = oldnp+1; i <= plainpoints.Size(); i++)
      TransformFromPlain (surfind, plainpoints[i], locpoints[i], h);
    pindex.SetSize(locpoints.Size());
    for (i = oldnp+1; i <= locpoints.Size(); i++)
    {
      TestPoint(locpoints[i],flag);
    }

    if (found)
    {
      ruleused[found]++;

      locpoints.SetSize (plainpoints.Size());
      for (i = oldnp+1; i <= plainpoints.Size(); i++)
        TransformFromPlain (surfind, plainpoints[i], locpoints[i], h);

      pindex.SetSize(locpoints.Size());

      for (i = oldnp+1; i <= locpoints.Size(); i++)
      {
        globind = SavePoint (locpoints[i]);
        pindex[i] = adfront -> AddPoint (locpoints[i], globind);
      }

      for (i = oldnl+1; i <= loclines.Size(); i++)
      {
        adfront -> AddLine (pindex[loclines[i].I1()],
                            pindex[loclines[i].I2()], surfind);
      }

      for (i = 1; i <= locelements.Size(); i++)
      {
        for (j = 1; j <= locelements[i].NP(); j++)
          locelements[i].PNum(j) =
            adfront -> GetGlobalIndex (pindex[locelements[i].PNum(j)]);


        locelements[i].SetSurfaceIndex (surfind);

        SaveElement (locelements[i]);
        cntelem++;
      }

      for (i = 1; i <= dellines.Size(); i++)
      {
        adfront -> DeleteLine (lindex[dellines[i]]);
      }

    }
    else
    {
      adfront -> IncrementClass (lindex[1]);
    }

    locpoints.SetSize(0);
    loclines.SetSize(0);
    pindex.SetSize(0);
    lindex.SetSize(0);
    delpoints.SetSize(0);
    dellines.SetSize(0);
    locelements.SetSize(0);


  }

  EndMesh ();
}



static void CalcTriangleBadness (double x2, double x3, double y3, int err2,
                                 double h, double & badness, double & g1x, double & g1y)
{
  // badness = sqrt(3) /36 * circumference^2 / area - 1 +
  //           h / li + li / h - 2

  // p1 = (0, 0), p2 = (x2, 0), p3 = (x3, y3);

  Vec2d v23;
  double l12, l13, l23, cir, area;
  static const double c = sqrt(3) / 36;
  double c1, c2, c3, c4;

  v23.X() = x3 - x2;
  v23.Y() = y3;

  l12 = x2;
  l13 = sqrt (x3*x3 + y3*y3);
  l23 = v23.Length();

  cir = l12 + l13 + l23;
  area = 0.5 * x2 * y3;

  if (area < 0)
  {
    g1x = 0;
    g1y = 0;
    badness = 1e10;
    return;
  }

  badness = c * cir * cir / area - 1;

  c1 = 2 * c * cir / area;
  c2 = 0.5 * c * cir * cir / (area * area);

  g1x = c1 * ( - 1 - x3 / l13) - c2 * (-v23.Y());
  g1y = c1 * (     - y3 / l13) - c2 * ( v23.X());

  if (err2)
  {
    badness += cir / h + h/l12 + h/l13 + h/l23 - 6;
    c3 = 1 / (h * l12) - h / (l12 * l12 * l12);
    c4 = 1 / (h * l13) - h / (l13 * l13 * l13);
    g1x -= c3 * x2 + c4 * x3;
    g1y -= c4 * y3;
  }
}



static Point3d sp1;
static Vec3d n, t1, t2;
static ARRAY<INDEX> locelements(0);
static ARRAY<int> locrots(0);
static ARRAY<Point3d> * optpoints;
static const ARRAY<Element> * optelements;
static int locerr2;
static double loch;
static int surfi, surfi2;


double Opti2FunctionValueGrad (const Vector & x, Vector & grad)
{
  int j, rot;
  INDEX eli;
  const Element * el;
  Vec3d n, v1, v2, e1, e2, vgrad;
  Point3d pp1;
  Vec2d g1, g2, g3;
  double badness, hbadness;

  vgrad.X() = 0;
  vgrad.Y() = 0;
  vgrad.Z() = 0;
  badness = 0;

  pp1 = sp1 + (x.Get(1) * t1) + (x.Get(2) * t2);
  meshthis -> ProjectPoint (surfi, pp1);
  meshthis -> GetNormalVector (surfi, pp1, n);

  for (j = 1; j <= locelements.Size(); j++)
  {
    eli = locelements.Get(j);
    rot = locrots.Get(j);
    el = &optelements->Get(eli);

    v1 = optpoints->Get(el->PNumMod(rot + 1)) - pp1;
    v2 = optpoints->Get(el->PNumMod(rot + 2)) - pp1;

    e1 = v1;
    e2 = v2;
    e1 /= e1.Length();
    e2 -= (e1 * e2) * e1;
    e2 /= e2.Length();

    if (Cross (e1, e2) * n > 0)
    {
      CalcTriangleBadness ( (e1 * v1), (e1 * v2), (e2 * v2), locerr2, loch,
                            hbadness, g1.X(), g1.Y());

      badness += hbadness;

      vgrad.X() += g1.X() * e1.X() + g1.Y() * e2.X();
      vgrad.Y() += g1.X() * e1.Y() + g1.Y() * e2.Y();
      vgrad.Z() += g1.X() * e1.Z() + g1.Y() * e2.Z();
    }
    else
      badness += 1e8;
  }

  vgrad -= (vgrad * n) * n;

  grad.Elem(1) = vgrad * t1;
  grad.Elem(2) = vgrad * t2;
  return badness;
}


double Opti2EdgeFunctionValueGrad (const Vector & x, Vector & grad)
{
  int j, rot;
  INDEX eli;
  const Element * el;
  Vec3d n1, n2, v1, v2, e1, e2, vgrad;
  Point3d pp1;
  Vec2d g1, g2, g3;
  double badness, hbadness;

  vgrad.X() = 0;
  vgrad.Y() = 0;
  vgrad.Z() = 0;
  badness = 0;

  pp1 = sp1 + x.Get(1) * t1;
  meshthis -> ProjectPoint2 (surfi, surfi2, pp1);

  for (j = 1; j <= locelements.Size(); j++)
  {
    eli = locelements.Get(j);
    rot = locrots.Get(j);
    el = &optelements->Get(eli);

    v1 = optpoints->Get(el->PNumMod(rot + 1)) - pp1;
    v2 = optpoints->Get(el->PNumMod(rot + 2)) - pp1;

    e1 = v1;
    e2 = v2;
    e1 /= e1.Length();
    e2 -= (e1 * e2) * e1;
    e2 /= e2.Length();

    CalcTriangleBadness ( (e1 * v1), (e1 * v2), (e2 * v2), locerr2, loch,
                          hbadness, g1.X(), g1.Y());

    badness += hbadness;

    vgrad.X() += g1.X() * e1.X() + g1.Y() * e2.X();
    vgrad.Y() += g1.X() * e1.Y() + g1.Y() * e2.Y();
    vgrad.Z() += g1.X() * e1.Z() + g1.Y() * e2.Z();
  }

  meshthis -> GetNormalVector (surfi, pp1, n1);
  meshthis -> GetNormalVector (surfi2, pp1, n2);

  v1 = Cross (n1, n2);
  v1 /= v1.Length();

  //  vgrad -= (vgrad * n) * n;
  //  grad.Elem(1) = vgrad * t1;
  //  grad.Elem(2) = vgrad * t2;

  grad.Elem(1) = (vgrad * v1) * (t1 * v1);

  return badness;
}






void Meshing2 :: ImproveMesh (ARRAY<Point3d> & points, const ARRAY<Element> & elements,
                              int improveedges, int numboundarypoints, double h, int steps, int err2)
{
  INDEX i, eli;
  int j, surfi3;
  const Element * el;

  Vec3d n1, n2;
  TABLE<INDEX> elementsonpoint(points.Size());
  Vector x(2), xedge(1);

  for (i = 1; i <= elements.Size(); i++)
    for (j = 1; j <= elements[i].NP(); j++)
      elementsonpoint.Add (elements[i].PNum(j), i);

  loch = h;
  locerr2 = err2;
  optpoints = &points;
  optelements = &elements;
  meshthis = this;

  if (improveedges)
    for (i = 1; i <= numboundarypoints; i++)
    {
      sp1 = points.Elem(i);

      locelements.SetSize(0);
      locrots.SetSize (0);
      surfi = surfi2 = surfi3 = 0;

      for (j = 1; j <= elementsonpoint.EntrySize(i); j++)
      {
        eli = elementsonpoint.Get(i, j);
        el = &elements.Get(eli);

        if (!surfi)
          surfi = el->SurfaceIndex();
        else if (surfi != el->SurfaceIndex())
        {
          if (surfi2 != 0 && surfi2 != el->SurfaceIndex())
            surfi3 = el->SurfaceIndex();
          else
            surfi2 = el->SurfaceIndex();
        }

        locelements.Append (eli);

        if (el->PNum(1) == i)
          locrots.Append (1);
        else if (el->PNum(2) == i)
          locrots.Append (2);
        else
          locrots.Append (3);
      }

      if (surfi2 && !surfi3)
      {
        GetNormalVector (surfi, sp1, n1);
        GetNormalVector (surfi2, sp1, n2);
        t1 = Cross (n1, n2);

        xedge = 0;
        //        BFGS (xedge, Opti2EdgeFunctionValueGrad);

        points.Elem(i).X() += xedge.Get(1) * t1.X();
        points.Elem(i).Y() += xedge.Get(1) * t1.Y();
        points.Elem(i).Z() += xedge.Get(1) * t1.Z();
        ProjectPoint2 (surfi, surfi2, points.Elem(i));
      }
    }


  for (i = numboundarypoints+1; i <= elementsonpoint.Size(); i++)
  {
    sp1 = points.Elem(i);

    locelements.SetSize(0);
    locrots.SetSize (0);
    for (j = 1; j <= elementsonpoint.EntrySize(i); j++)
    {
      eli = elementsonpoint.Get(i, j);
      el = &elements.Get(eli);
      surfi = el->SurfaceIndex();

      locelements.Append (eli);

      if (el->PNum(1) == i)
        locrots.Append (1);
      else if (el->PNum(2) == i)
        locrots.Append (2);
      else
        locrots.Append (3);
    }

    GetNormalVector (surfi, sp1, n);
    if (fabs (n.X()) > 0.3)
    {
      t1.X() = n.Y();
      t1.Y() = -n.X();
      t1.Z() = 0;
      t1 /= t1.Length();
    }
    else
    {
      t1.X() = 0;
      t1.Y() = -n.Z();
      t1.Z() = n.Y();
      t1 /= t1.Length();
    }
    t2 = Cross (n, t1);

    x = 0;
    //    BFGS (x, Opti2FunctionValueGrad);

    points.Elem(i).X() += (x.Get(1) * t1.X() + x.Get(2) * t2.X());
    points.Elem(i).Y() += (x.Get(1) * t1.Y() + x.Get(2) * t2.Y());
    points.Elem(i).Z() += (x.Get(1) * t1.Z() + x.Get(2) * t2.Z());
    ProjectPoint (surfi, points.Elem(i));
  }
}
