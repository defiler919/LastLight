"""GPU unit probe of the shipped alpha expression and shipped local ramp.
Only transient offscreen materials/targets: never binds a test material to a
cabinet or creates any geometry. The PIE screenshots test world coordinates.
"""
import unreal
import create_darkwell_project_fog_materials as m

def probe(world,report):
    lib=unreal.MaterialEditingLibrary; render=unreal.RenderingLibrary
    material=unreal.load_asset('/Game/Darkwell/Vision/PropLab/M_ManualFixedReveal')
    shadow=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_OPACITY_MASK)
    assert isinstance(shadow,unreal.MaterialExpressionShadowReplace)
    inputs=lib.get_inputs_for_material_expression(material,shadow)
    dither=inputs[0]
    alpha=lib.get_inputs_for_material_expression(material,dither)[0]
    assert isinstance(alpha,unreal.MaterialExpressionCustom)
    code=alpha.get_editor_property('code')
    # Recompile the actual shipped alpha in a transient UV test fixture. This
    # permits float readback before temporal dithering, not a CPU mirror test.
    test=unreal.new_object(unreal.Material)
    test.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=m.expr(test,unreal.MaterialExpressionTextureCoordinate,-900,0)
    def tex(name,y):
        n=m.expr(test,unreal.MaterialExpressionTextureSampleParameter2D,-600,y)
        n.set_editor_property('parameter_name',name)
        n.set_editor_property('texture',unreal.load_asset('/Engine/EngineMaterials/DefaultBloomKernel'))
        n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        m.connect(uv,'',n,'Coordinates');return n
    output=m.custom_expression(test,code,[('Raw',tex('Raw',0),'R'),('Soft',tex('Soft',200),'R'),
        ('Enabled',m.scalar_parameter(test,'Enabled',1,-600,400)),('Ready',m.scalar_parameter(test,'Ready',1,-600,500))],-100,0,'Actual shipped reveal alpha GPU probe')
    lib.connect_material_property(output,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(test)
    field=unreal.new_object(unreal.Material)
    field.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=m.expr(field,unreal.MaterialExpressionTextureCoordinate,-700,0)
    output=m.custom_expression(field,'return (Reverse > 0.5 ? 1.0-UV.x : UV.x) < Fraction ? Level : 0.0;',[
        ('UV',uv),('Fraction',m.scalar_parameter(field,'Fraction',0,-500,200)),
        ('Reverse',m.scalar_parameter(field,'Reverse',0,-500,300)),('Level',m.scalar_parameter(field,'Level',1,-500,400))],-100,0,'Synthetic legal coverage test input')
    lib.connect_material_property(output,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.recompile_material(field)
    ramp=unreal.load_asset('/Game/Darkwell/Vision/PropLab/M_ManualFixedRevealRamp')
    def mid(parent):return unreal.MaterialLibrary.create_dynamic_material_instance(world,parent)
    tm,fm,rm=mid(test),mid(field),mid(ramp)
    def target():return render.create_render_target2d(world,64,4,unreal.TextureRenderTargetFormat.RTF_RGBA16F)
    raw,out=target(),target(); history=[target(),target()]
    tm.set_texture_parameter_value('Raw',raw);rm.set_texture_parameter_value('Raw',raw)
    fm.set_scalar_parameter_value('Level',1)
    yield 3 # allow asynchronous shader compilation to finish before draws
    count=0
    for reverse in (0,1):
        expected=[0.0]*64
        for t in history:render.clear_render_target2d(world,t)
        old=0;fm.set_scalar_parameter_value('Reverse',reverse)
        for fraction in (0,.03125,.25,.5,.75,1):
            fm.set_scalar_parameter_value('Fraction',fraction)
            render.draw_material_to_render_target(world,raw,fm)
            for local_step in range(4):
                rm.set_texture_parameter_value('Previous',history[old]);rm.set_scalar_parameter_value('Step',.25)
                old=1-old;render.draw_material_to_render_target(world,history[old],rm)
                tm.set_texture_parameter_value('Soft',history[old]);render.draw_material_to_render_target(world,out,tm)
                yield 0
                pixels=render.read_render_target_raw_pixel_area(world,out,0,1,64,1,False)
                assert len(pixels)==64,len(pixels)
                for x,p in enumerate(pixels):
                    u=(x+.5)/64;legal=(1-u if reverse else u)<fraction
                    expected[x]=min(1,expected[x]+.25) if legal else 0
                    assert abs(p.r-expected[x])<.002,(reverse,fraction,local_step,x,p.r,expected[x])
                report.append(dict(reverse=reverse,fraction=fraction,step=local_step,alpha=[p.r for p in pixels]));count+=1
        # Existing soft history must never leak when raw is illegal, or unbound.
        for label,level,ready,enabled in (('illegal',.5,1,1),('not_ready',1,0,1),('legacy_bypass',0,0,0)):
            fm.set_scalar_parameter_value('Level',level);fm.set_scalar_parameter_value('Fraction',1)
            render.draw_material_to_render_target(world,raw,fm)
            tm.set_scalar_parameter_value('Ready',ready);tm.set_scalar_parameter_value('Enabled',enabled)
            render.draw_material_to_render_target(world,out,tm);yield 0
            pixels=render.read_render_target_raw_pixel_area(world,out,0,1,64,1,False)
            wanted=1 if label=='legacy_bypass' else 0
            assert len(pixels)==64 and all(abs(p.r-wanted)<.002 for p in pixels),label
            report.append(dict(reverse=reverse,case=label,expected=wanted));count+=1
        fm.set_scalar_parameter_value('Level',1);tm.set_scalar_parameter_value('Ready',1);tm.set_scalar_parameter_value('Enabled',1)
    unreal.log(f'FIXED_REVEAL_SHADER_PASS cases={count} pixelsPerCase=64 percentages=0,narrow,25,50,75,100')
