#!/usr/bin/env python3
"""Normalize pinned official Stability contracts; emit an apply_patch patch.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).
Inputs are the downloaded v2 document and v1OpenAPISpec extracted from the
official platform asset. No credentials or network are used by this importer.
"""
import copy
import difflib
import hashlib
import json
import pathlib
import re
import sys

V2_SHA = '1ea5455da5ce2efef103daa142cc44a3e70b4fb3cfcdfa0c9a6a6f46d93912ea'
V1_SHA = '779a5d5252a6c093441a5d64155af4462a50905100d8cf2cdca20b1e032785d8'


def load(path, expected):
    raw = pathlib.Path(path).read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError('source checksum changed; review a new vendor revision explicitly')
    return json.loads(raw)


def walk(value, transform):
    if isinstance(value, dict):
        return transform({k: walk(v, transform) for k, v in value.items() if k != 'x-codeSamples'})
    if isinstance(value, list):
        return [walk(v, transform) for v in value]
    return value


def normalize(v2, v1):
    result = walk(copy.deepcopy(v2), lambda v: v)
    # The reference describes these as compatibility aliases; expose their v2beta equivalents.
    excluded = {p: 'Compatibility alias of the corresponding v2beta image operation.'
                for p in result['paths'] if p.startswith('/v2alpha/')}
    for p in excluded:
        del result['paths'][p]

    def prefix(v):
        if 'discriminator' in v:
            mapping = v['discriminator'].get('mapping', {})
            for name, ref in mapping.items():
                if ref.startswith('#/components/'):
                    parts = ref.split('/')
                    parts[-1] = 'V1_' + parts[-1]
                    mapping[name] = '/'.join(parts)
        if '$ref' in v and v['$ref'].startswith('#/components/'):
            parts = v['$ref'].split('/')
            parts[-1] = 'V1_' + parts[-1]
            v['$ref'] = '/'.join(parts)
        return v

    legacy = walk(copy.deepcopy(v1), prefix)
    result['paths'].update(legacy['paths'])
    for category, entries in legacy['components'].items():
        result['components'].setdefault(category, {}).update({'V1_' + k: v for k, v in entries.items()})
    # Security names are map keys rather than JSON pointers.
    for path in legacy['paths']:
        for operation in result['paths'][path].values():
            if isinstance(operation, dict) and 'security' in operation:
                operation['security'] = [{'V1_' + k: v for k, v in item.items()}
                                         for item in operation['security']]
    repairs = []
    sd3 = result['paths']['/v2beta/stable-image/generate/sd3']['post']['requestBody']['content']['multipart/form-data']['schema']['properties']['model']
    assert 'sd3.5-flash' in sd3['description'] and 'sd3.5-flash' not in sd3['enum']
    sd3['enum'].append('sd3.5-flash')
    repairs.append({'operation': 'POST /v2beta/stable-image/generate/sd3',
                    'reason': 'The same official parameter description and pricing document SD3.5 Flash.',
                    'change': 'Include the documented sd3.5-flash model omitted from the enum.'})
    mask = result['components']['schemas']['V1_MaskSource']
    mask['enum'] = ['MASK_IMAGE_WHITE', 'MASK_IMAGE_BLACK', 'INIT_IMAGE_ALPHA']
    repairs.append({'operation': 'POST /v1/generation/{engine_id}/image-to-image/masking',
                    'reason': 'The parameter description exhaustively defines three mask sources.',
                    'change': 'Declare the three documented mask sources as an enum.'})
    outpaint = result['paths']['/v2beta/stable-image/edit/outpaint']['post']['requestBody']['content']['multipart/form-data']['schema']['properties']['creativity']
    assert outpaint['allOf'][1]['minimum'] == 0 and outpaint['allOf'][1]['maximum'] == 1
    result['paths']['/v2beta/stable-image/edit/outpaint']['post']['requestBody']['content']['multipart/form-data']['schema']['properties']['creativity'] = {
        **result['components']['schemas']['Creativity'], **outpaint['allOf'][1]}
    repairs.append({'operation': 'POST /v2beta/stable-image/edit/outpaint',
                    'reason': 'The operation explicitly documents creativity 0 through 1; allOf incorrectly retains the upscale range.',
                    'change': 'Apply the operation-specific range to a copy of Creativity.'})
    for path in ('/v1/generation/{engine_id}/image-to-image', '/v1/generation/{engine_id}/image-to-image/masking'):
        result['paths'][path]['post']['requestBody']['content']['multipart/form-data']['encoding'] = {
            'text_prompts': {'style': 'deepObject', 'explode': True}}
    repairs.append({'operations': 'Legacy image-to-image and masking',
                    'reason': 'Official multipart examples use text_prompts[0][text] and text_prompts[0][weight].',
                    'change': 'Declare deepObject encoding for multipart text_prompts.'})
    result['components']['schemas']['Seed']['type'] = 'integer'
    repairs.append({'schema': 'Seed', 'reason': 'A random seed is an integer in the documented uint32 range.',
                    'change': 'Narrow the numeric seed to integer; preserve zero as a random-seed request.'})
    erase = result['paths']['/v2beta/stable-image/edit/erase']['post']['requestBody']['content']['multipart/form-data']['schema']
    assert erase['required'] == ['image', 'prompt'] and 'prompt' not in erase['properties']
    erase['required'] = ['image']
    repairs.append({'operation': 'POST /v2beta/stable-image/edit/erase',
                    'reason': 'Official erase fields and examples require image and optional mask, not a nonexistent prompt.',
                    'change': 'Remove nonexistent prompt from required.'})
    error = result['components']['schemas']['V1_Error']
    assert error['required'] == ['name', 'id', 'message', 'status'] and 'status' not in error['properties']
    error['required'].remove('status')
    repairs.append({'schema': 'V1_Error',
                    'reason': 'The official legacy error schema and examples define id, name, and message only.',
                    'change': 'Remove nonexistent status from required.'})
    for path, methods in result['paths'].items():
        for method, operation in methods.items():
            if method not in ('get', 'post'):
                continue
            parameters = operation.get('parameters', [])
            operation['parameters'] = [p for p in parameters
                if p.get('$ref') != '#/components/parameters/FormDataContentType']
    repairs.append({'parameter': 'FormDataContentType',
                    'reason': 'Official documentation requires the HTTP multipart serializer to generate the boundary.',
                    'change': 'Remove redundant required content-type header parameter; requestBody owns serialization.'})
    result['info']['title'] = 'Stability AI reviewed REST contract'
    result['info']['version'] = '2026-09-19.1'
    result['servers'] = [{'url': 'https://api.stability.ai'}]
    result['x-qore-normalized'] = {'version': 1, 'repairs': repairs, 'excluded_paths': excluded,
        'changes': ['Namespace legacy components with V1_.', 'Remove documentation code samples.',
                    'Select canonical production origin.']}
    # Restrict components to the transitive references of the exported paths.
    used = set()

    def visit(value):
        if isinstance(value, str) and value.startswith('#/components/'):
            visit({'$ref': value}) if value not in used else None
        if isinstance(value, dict):
            ref = value.get('$ref')
            if ref and ref.startswith('#/components/') and ref not in used:
                used.add(ref)
                target = result
                for part in ref.split('/')[1:]:
                    target = target[part.replace('~1', '/').replace('~0', '~')]
                visit(target)
            for item in value.values():
                visit(item)
        elif isinstance(value, list):
            for item in value:
                visit(item)

    visit(result['paths'])
    result['components'] = {category: {name: value for name, value in entries.items()
        if category == 'securitySchemes' or '#/components/' + category + '/' + name in used}
        for category, entries in result['components'].items()}
    return result


def presentation(contract):
    """Generate overlays only at declared property/choice positions, never inside example values."""
    labels = {
        'sd3.5-large': 'SD3.5 Large', 'sd3.5-large-turbo': 'SD3.5 Large Turbo',
        'sd3.5-medium': 'SD3.5 Medium', 'sd3.5-flash': 'SD3.5 Flash',
        'stable-audio-2': 'Stable Audio 2', 'stable-audio-2.5': 'Stable Audio 2.5',
        'stable-audio-3': 'Stable Audio 3', 'png': 'PNG', 'jpeg': 'JPEG', 'webp': 'WebP',
        'mp3': 'MP3', 'wav': 'WAV', 'glb': 'GLB', '3d-model': '3D Model',
        '1:1': '1:1 (Square)', '16:9': '16:9 (Landscape Video)', '9:16': '9:16 (Portrait Video)',
        '5:4': '5:4 (Standard)', '4:5': '4:5 (Portrait)', '21:9': '21:9 (Cinemascope)',
        '9:21': '9:21 (Tall Cinemascope)', '2:3': '2:3 (Tall Portrait)', '3:2': '3:2 (Wide Landscape)',
        'MASK_IMAGE_WHITE': 'Replace White Mask Pixels', 'MASK_IMAGE_BLACK': 'Replace Black Mask Pixels',
        'INIT_IMAGE_ALPHA': 'Replace Transparent Image Pixels',
        'IMAGE_STRENGTH': 'Image Strength', 'STEP_SCHEDULE': 'Diffusion Step Schedule',
        'triangle': 'Triangular Faces', 'quad': 'Quadrilateral Faces', 'none': 'No Remeshing',
        'DDIM': 'DDIM', 'DDPM': 'DDPM', 'K_DPMPP_2M': 'DPM++ 2M',
        'K_DPMPP_2S_ANCESTRAL': 'DPM++ 2S Ancestral', 'K_DPM_2': 'DPM 2',
        'K_DPM_2_ANCESTRAL': 'DPM 2 Ancestral', 'K_EULER': 'Euler',
        'K_EULER_ANCESTRAL': 'Euler Ancestral', 'K_HEUN': 'Heun', 'K_LMS': 'LMS',
    }
    names = {'cfg_scale': 'Prompt Guidance', 'init_image': 'Initial Image', 'text_prompts': 'Weighted Prompts',
             'mask_start': 'Mask Start (Seconds)', 'mask_end': 'Mask End (Seconds)',
             'duration': 'Duration (Seconds)', 'model': 'Model', 'remesh': 'Mesh Faces',
             'vertex_count': 'Target Vertex Count', 'pointcloud': 'Point Cloud'}

    def resolve(node):
        if '$ref' in node:
            value = contract
            for part in node['$ref'].split('/')[1:]:
                value = value[part]
            return resolve(value)
        result = {}
        properties = {}
        for item in node.get('allOf', []):
            child = resolve(item)
            result.update(child)
            properties.update(child.get('properties', {}))
        result.update(node)
        properties.update(node.get('properties', {}))
        if properties:
            result['properties'] = properties
        return result

    result = {}
    for path, methods in contract['paths'].items():
        for method, operation in methods.items():
            if 'requestBody' not in operation:
                continue
            body = resolve(next(iter(resolve(operation['requestBody'])['content'].values()))['schema'])
            fields = {}
            for name, schema in body.get('properties', {}).items():
                schema = resolve(schema)
                title = names.get(name, name.replace('_', ' ').title())
                field = {'display_name': title, 'short_desc': title,
                         'preselected': name in body.get('required', []) or name in ('model', 'output_format')}
                description = schema.get('description', '')
                if description:
                    parts = re.split(r'(```[\s\S]*?```|`[^`]*`|https?://[^\s)]+)', description)
                    description = ''.join(part if i % 2 else re.sub(r'\b[a-z][a-z0-9]*(?:_[a-z0-9]+)+\b',
                        lambda match: '`' + match.group() + '`', part) for i, part in enumerate(parts))
                    if len(description) > 500 and '**' not in description:
                        description = '**Usage**\n\n' + ('' if re.search(r'^- ', description, re.M) else '- ') + description
                    elif len(description) > 500 and not re.search(r'^\s*[-*] ', description, re.M):
                        description = '- ' + description
                    field['desc'] = description
                if 'enum' in schema:
                    field['allowed_values'] = [{'value': v, 'display_name': labels.get(v,
                        str(v).replace('-', ' ').replace('_', ' ').title())} for v in schema['enum']]
                if name == 'keep_original_background':
                    field['allowed_values'] = [{'value': True, 'display_name': 'Keep Background'},
                                               {'value': False, 'display_name': 'Replace Background'}]
                if not schema.get('description'):
                    field['desc'] = {'mask_start': 'Start of the replaced audio range, in seconds.',
                                     'mask_end': 'End of the replaced audio range, in seconds.',
                                     'clip_guidance_preset': 'Choose a legacy CLIP guidance preset.'}.get(name, title + '.')
                if name == 'prompt':
                    field['example_value'] = 'A blue ceramic mug on a plain studio background.' if '/audio/' not in path else 'A short, clear bell tone.'
                if name == 'text_prompts':
                    field['desc'] = 'One or more prompts with optional weights. Use negative weights for content to avoid.'
                fields['body.' + name] = field
            if path.endswith('/generate/sd3'):
                for name in ('image', 'strength'):
                    fields['body.' + name]['depends_on'] = ['mode=image-to-image']
                fields['body.aspect_ratio']['depends_on'] = ['mode=text-to-image']
                fields['body.mode']['has_dependents'] = True
            if path.endswith('/replace-background-and-relight'):
                for name, other in [('background_prompt', 'background_reference'), ('background_reference', 'background_prompt')]:
                    fields['body.' + name].update(required_groups=['background'], exclusive_with=[other], preselected=True)
                for name, other in [('light_reference', 'light_source_direction'), ('light_source_direction', 'light_reference')]:
                    fields['body.' + name]['exclusive_with'] = [other]
            if path.endswith('/image-to-image'):
                fields['body.init_image_mode']['has_dependents'] = True
                fields['body.image_strength']['depends_on'] = ['init_image_mode=IMAGE_STRENGTH']
                for name in ('step_schedule_start', 'step_schedule_end'):
                    fields['body.' + name]['depends_on'] = ['init_image_mode=STEP_SCHEDULE']
            if '/stable-audio-2/' in path and 'body.model' in fields:
                fields['body.model']['has_dependents'] = True
                fields['body.steps']['depends_on'] = ['model']
            result[method.upper() + ' ' + path] = fields
    return result


def main():
    v2 = load(sys.argv[1], V2_SHA)
    v1 = load(sys.argv[2], V1_SHA)
    contract = normalize(v2, v1)
    provenance = {
        'copyright': 'Stability AI. The official artifacts declare no schema-specific license.',
        'attribution': 'API descriptions and schemas from the official Stability AI developer platform.',
        'terms': 'https://platform.stability.ai/legal/terms-of-service',
        'retrieved': '2026-09-19', 'normalization_version': 1,
        'v2': {'url': 'https://api.stability.ai/v2alpha/openapi', 'sha256': V2_SHA,
               'bytes': 228110, 'openapi': v2['openapi'], 'version': v2['info']['version'], 'operations': 31},
        'v1': {'url': 'https://platform.stability.ai/assets/index-eklWmhUz.js',
               'extraction': 'Evaluate only const openapi through v1OpenAPISpec; JSON.stringify(v1OpenAPISpec).',
               'extracted_json_sha256': V1_SHA, 'openapi': v1['openapi'], 'version': 'v1', 'operations': 6},
        'operations': [{'method': m.upper(), 'path': p} for p, methods in contract['paths'].items()
                       for m in methods if m in ('get', 'post')],
    }
    print('*** Begin Patch')
    for name, value in [('stability-openapi.json', contract), ('stability-provenance.json', provenance),
                        ('stability-presentation.json', presentation(contract))]:
        path = pathlib.Path('qlib/StabilityAiRestClient') / name
        output = json.dumps(value, indent=2, ensure_ascii=False) + '\n'
        if path.exists():
            difference = list(difflib.unified_diff(path.read_text().splitlines(), output.splitlines()))
            if not difference:
                continue
            print('*** Update File: ' + str(path))
            for line in difference[2:]:
                print('@@' if line.startswith('@@') else line)
        else:
            print('*** Add File: ' + str(path))
            for line in output.splitlines():
                print('+' + line)
    print('*** End Patch')


if __name__ == '__main__':
    main()
