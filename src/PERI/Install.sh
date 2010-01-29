# Install/unInstall package files in LAMMPS

if (test $1 == 1) then

  cp atom_vec_peri.cpp ..
  cp pair_peri_pmb.cpp ..
  cp fix_peri_neigh.cpp ..
  cp compute_damage_atom.cpp ..

  cp atom_vec_peri.h ..
  cp pair_peri_pmb.h ..
  cp fix_peri_neigh.h ..
  cp compute_damage_atom.h ..

elif (test $1 == 0) then

  rm -f ../atom_vec_peri.cpp
  rm -f ../pair_peri_pmb.cpp
  rm -f ../fix_peri_neigh.cpp
  rm -f ../compute_damage_atom.cpp

  rm -f ../atom_vec_peri.h
  rm -f ../pair_peri_pmb.h
  rm -f ../fix_peri_neigh.h
  rm -f ../compute_damage_atom.h

fi
