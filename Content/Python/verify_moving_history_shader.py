"""GPU readback of actual moving-history textures through the shipped final gate.

Only transient materials/targets. No coverage, D/V/R, geometry or scene material
is replaced. The probe recompiles the loaded production expression verbatim.
"""
import unreal
import create_darkwell_project_fog_materials as m


def probe(world, report):
    lib, render = unreal.MaterialEditingLibrary, unreal.RenderingLibrary
    source = unreal.load_asset('/Game/Darkwell/Vision/PropLab/M_MovingAccumulatedMemory')
    alpha = lib.get_material_property_input_node(source, unreal.MaterialProperty.MP_OPACITY)
    code = alpha.get_editor_property('code')
    assert 'Ownership.Load' in code and 'saturate(State.b) *' in code
    test = unreal.new_object(unreal.Material)
    test.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    uv0 = m.expr(test, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    offset = m.vector_parameter(test, 'ProbeOffset', unreal.LinearColor(0,0,0,0), -1000, 200)
    uv = m.binary(test, unreal.MaterialExpressionAdd, uv0, m.mask(test,offset,'rg',-800,200), -700, 0)
    sample = m.expr(test, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    sample.set_editor_property('parameter_name', 'SpatialStateTexture')
    sample.set_editor_property('texture', unreal.load_asset('/Engine/EngineMaterials/DefaultBloomKernel'))
    sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    m.connect(uv, '', sample, 'Coordinates')
    tex = m.expr(test, unreal.MaterialExpressionTextureObjectParameter, -500, 300)
    tex.set_editor_property('parameter_name', 'SpatialStateTexture')
    tex.set_editor_property('texture', sample.get_editor_property('texture'))
    tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    ready = m.scalar_parameter(test, 'SpatialReady', 1, -500, 500)
    final = m.custom_expression(test, code, [('State',sample,'RGB'),('Ownership',tex),('UV',uv),('Ready',ready)],0,0,'Verbatim shipped final opacity')
    diagnostic = m.custom_expression(test, '''uint W,H; Ownership.GetDimensions(W,H);
int2 P=clamp(int2(floor(UV*float2(W,H))),int2(0,0),int2(W,H)-1);
return float3(Final.r,State.b,Ownership.Load(int3(P,0)).a);''',
        [('Final',final),('State',sample,'RGB'),('Ownership',tex),('UV',uv)],200,0,'R=actual final G=smooth B=hard input')
    diagnostic.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    lib.connect_material_property(diagnostic,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(test)
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world,test)
    target = render.create_render_target2d(world,128,128,unreal.TextureRenderTargetFormat.RTF_RGBA16F)
    proxies = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
               if 'SpatialMemory_Lab.InWorld.Rotate.Cabinet' in a.get_name()]
    textures = []
    for actor in proxies:
        parts = actor.get_components_by_class(unreal.StaticMeshComponent)
        if parts:
            actual_mid = parts[0].get_material(0)
            assert actual_mid.get_editor_property('parent') == source, 'Probe must match the actual proxy material'
            texture = actual_mid.get_texture_parameter_value('SpatialStateTexture')
            if texture and texture not in textures:
                textures.append(texture)
    assert textures, 'Need real retained spatial-history texture input'
    yield 3
    checked = blocked_positive = allowed_positive = 0
    for texture in textures:
        mid.set_texture_parameter_value('SpatialStateTexture',texture)
        for dx,dy in ((0,0),(.001,.002),(-.001,-.002),(.003,-.003)):
            mid.set_vector_parameter_value('ProbeOffset',unreal.LinearColor(dx,dy,0,0))
            render.draw_material_to_render_target(world,target,mid)
            yield .1
            pixels = render.read_render_target_raw_pixel_area(world,target,0,0,128,128,False)
            assert len(pixels)==16384, len(pixels)
            for p in pixels:
                checked += 1
                if p.b < .5:
                    assert p.r == 0, ('hard ownership leakage',p)
                    blocked_positive += p.g > .001
                else:
                    assert abs(p.r-p.g)<.002, ('legal history was lost',p)
                    allowed_positive += p.g > .001
    assert blocked_positive > 0 and allowed_positive > 0, (blocked_positive,allowed_positive)
    report.append(dict(shader=source.get_path_name(),textures=len(textures),pixels=checked,
                       blocked_positive=blocked_positive,allowed_positive=allowed_positive,failures=0))
    unreal.log('MOVING_HISTORY_FINAL_SHADER_PASS ' + str(report[-1]))
    render.release_render_target2d(target)
