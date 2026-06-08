TopoDS_Shape make_shape_compound(const std::vector<TopoDS_Shape>& shapes) {
  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);
  bool added = false;
  for (const auto& shape : shapes) {
    if (!shape.IsNull()) {
      builder.Add(compound, shape);
      added = true;
    }
  }
  return added ? TopoDS_Shape(compound) : TopoDS_Shape{};
}
