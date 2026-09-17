// Safety net: delete coincident constraints whose merged point died
// or whose listed lines no longer reference it (shared helper — the
// old inline sweep compared LINE targets against the live POINT set
// and silently erased every coincident constraint).
sweep_orphan_coincident_constraints(params);
