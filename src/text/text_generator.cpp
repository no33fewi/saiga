#include "text/text_generator.h"
#include "libhello/opengl/texture/texture.h"
#include <algorithm>
#include <FreeImagePlus.h>
#include <ft2build.h>
#include <freetype2/freetype/ftstroke.h>
#include FT_FREETYPE_H

#define NOMINMAX
#undef max
#undef min

FT_Library TextGenerator::ft = nullptr;

TextGenerator::TextGenerator(){

    if(ft==nullptr){
        //        ft = FT_Library> (new FT_Library());
        if(FT_Init_FreeType(&ft)) {
            cerr<< "Could not init freetype library"<<endl;
            exit(1);
        }
    }
}

TextGenerator::~TextGenerator()
{
    FT_Done_Face(face);
    delete textureAtlas;
}


void TextGenerator::loadFont(const std::string &font, int font_size, int stroke_size){
    this->font = font;
    this->font_size = font_size;
    this->stroke_size = stroke_size;

    //    face = std::make_shared<FT_Face>();
    if(FT_New_Face(ft, font.c_str(), 0, &face)) {
        cerr<<"Could not open font "<<font<<endl;
        //        assert(0);
        return;
    }
    FT_Set_Pixel_Sizes(face, 0, font_size);

    createTextureAtlas();
}

void TextGenerator::createTextureAtlas(){


    FT_Error error;
    FT_Stroker stroker;

    if(stroke_size>0){
        // Set up a stroker.

        FT_Stroker_New(ft, &stroker);
        FT_Stroker_Set(stroker,
                       stroke_size,
                       FT_STROKER_LINECAP_ROUND,
                       FT_STROKER_LINEJOIN_ROUND,
                       0);
    }


    charPaddingX = 2;
    charPaddingY = 2;
    charBorder = 0;

    int chars= 0;
    int w=0,h=0;


    const int count = 128-32;
    FT_Glyph glyphs[count];
    FT_Glyph glyph_strokes[count];

    FT_GlyphSlot slot = (face)->glyph;
    for(int i = 32; i < 128; i++) {
        FT_UInt  glyph_index;

        /* retrieve glyph index from character code */
        glyph_index = FT_Get_Char_Index( face, i );

        /* load glyph image into the slot (erase previous one) */
        error = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
        if ( error )
            continue;  /* ignore errors */

        //copy glyph
        error = FT_Get_Glyph( slot, &glyphs[i-32] );
        /* render the glyph to a bitmap, don't destroy original */
        error = FT_Glyph_To_Bitmap( &glyphs[i-32], FT_RENDER_MODE_NORMAL, NULL, 0 );

        FT_Glyph glyph = glyphs[i-32];

        if(stroke_size>0){
            error = FT_Get_Glyph( slot, &glyph_strokes[i-32] );
            error = FT_Glyph_Stroke( &glyph_strokes[i-32], stroker, 1 );
            error = FT_Glyph_To_Bitmap( &glyph_strokes[i-32], FT_RENDER_MODE_NORMAL, NULL, 0 );
            glyph = glyph_strokes[i-32];
        }





        if ( glyph->format != FT_GLYPH_FORMAT_BITMAP )
            cout<< "invalid glyph format returned!" <<endl;

        FT_BitmapGlyph bitmap = (FT_BitmapGlyph)glyph;
        FT_Bitmap* source = &bitmap->bitmap;

        character_info &info = characters[i];

        info.ax = ( glyph->advance.x + 0x8000 ) >> 16;
        info.ay = ( glyph->advance.y + 0x8000 ) >> 16;
        info.ax += stroke_size/64; //????

        info.bw = source->width;
        info.bh = source->rows;

        info.bl = bitmap->left;
        info.bt = bitmap->top;


        maxCharacter.min = glm::min(maxCharacter.min,vec3(info.bl,info.bt-info.bh,0));
        maxCharacter.max = glm::max(maxCharacter.max,vec3(info.bl+info.bw,info.bt-info.bh+info.bh,0));

        chars++;


    }

    if(stroke_size>0){
    FT_Stroker_Done(stroker);
    }


    int charsPerRow = glm::ceil(glm::sqrt((float)chars));
    //    cout<<"chars "<<chars<<" charsperrow "<<charsPerRow<<" total "<<charsPerRow*charsPerRow<<endl;

    int atlasHeight = 0;
    int atlasWidth = 0;

    for(int cy = 0 ; cy < charsPerRow ; ++cy){
        int currentW = 0;
        int currentH = 0;

        for(int cx = 0 ; cx < charsPerRow ; ++cx){
            int i = cy * charsPerRow + cx;
            if(i>=chars)
                break;
            character_info &info = characters[i+32];

            info.atlasX = currentW;
            info.atlasY = atlasHeight;

            currentW += info.bw+charPaddingX;
            currentH = std::max(currentH, info.bh);
        }
        atlasWidth = std::max(currentW, atlasWidth);
        atlasHeight += currentH+charPaddingY;
    }

    cout<<"AtlasWidth "<<atlasWidth<<" AtlasHeight "<<atlasHeight<<endl;

    h = atlasHeight;
    w = atlasWidth;

    //increase width to a number dividable by 8 to fix possible alignment issues
    while(w%8!=0)
        w++;

    Image img;
    img.bitDepth = 8;
    img.channels = 1;
    img.width = w;
    img.height = h;
    img.create();
    img.makeZero();


    img.addChannel();





    for(int i = 32; i < 128; i++) {

        FT_BitmapGlyph bitmap = (FT_BitmapGlyph)glyphs[i-32];
        FT_Bitmap* source = &bitmap->bitmap;




        character_info &info = characters[i];


        float tx = (float)info.atlasX / (float)w;
        float ty = (float)info.atlasY / (float)h;

        info.tcMin = vec2(tx,ty);
        info.tcMax = vec2(tx+(float)info.bw/(float)w,ty+(float)info.bh/(float)h);

        //        textureAtlas->uploadSubImage(info.atlasX, info.atlasY, info.bw, info.bh, g->bitmap.buffer);
        //        img.setSubImage(info.atlasX, info.atlasY, info.bw, info.bh, g->bitmap.buffer);
        //        cout<<"left "<<bitmap->left<<" "<<bitmapstroke->left<<endl;
        //        cout<<"top "<<bitmap->top<<" "<<bitmapstroke->top<<endl;

        //offset from normal glyph relative to stroke glyph
        int offsetX=0,offsetY=0;
        if(stroke_size>0){
            FT_BitmapGlyph bitmapstroke = (FT_BitmapGlyph)glyph_strokes[i-32];
            FT_Bitmap* sourceStroke = &bitmapstroke->bitmap;
            offsetX = bitmap->left - bitmapstroke->left;
            offsetY = -bitmap->top + bitmapstroke->top;

            //        cout<<offsetX<<" "<<offsetY<<endl;

            for(int y = 0 ; y < info.bh  ; ++y){
                for(int x = 0 ; x < info.bw ; ++x){
                    unsigned char c = sourceStroke->buffer[y*(info.bw) + x];
                    uint16_t s = c;
                    img.setPixel(info.atlasX+x ,info.atlasY+y,s);
                }
            }
        }

        for(int y = 0 ; y < source->rows  ; ++y){
            for(int x = 0 ; x < source->width ; ++x){

                unsigned char c = source->buffer[y*(source->width) + x];
                uint16_t s = c;
                //                uint16_t s = 0;

                int ox = x + offsetX;
                int oy = y + offsetY;
                //                if(ox>=0 && ox<source->width && oy>=0 && oy <source->rows){
                //                    unsigned char c = source->buffer[oy*(source->width) + ox];
                //                    s = s + c<<8;
                //                }

                uint16_t old = img.getPixel<uint16_t>(info.atlasX+ox ,info.atlasY+oy);
                old = old + (s<<8);
                //                old = s;
                img.setPixel(info.atlasX+ox ,info.atlasY+oy,old);
            }
        }

    }


    if(stroke_size>0){
        for(int i = 32; i < 128; i++) {

            FT_Done_Glyph(glyph_strokes[i-32]);
        }
    }
    for(int i = 32; i < 128; i++) {
        FT_Done_Glyph(glyphs[i-32]);
    }
    img.addChannel();


    fipImage fipimage;
    img.convertTo(fipimage);
    std::string str = "debug/test"+std::to_string(w)+".png";
    if(!fipimage.save(str.c_str())){
        cout<<"could not save "<<str<<endl;
    }

    //    std::vector<GLubyte> data(w*h,0x00);
    textureAtlas = new Texture();

    //zero initialize texture
    //        textureAtlas->createTexture(w ,h,GL_RED, GL_R8  ,GL_UNSIGNED_BYTE,img.data);
    //    textureAtlas->createTexture(w ,h,GL_RED, GL_R8  ,GL_UNSIGNED_BYTE,0);

    textureAtlas->fromImage(img);

    textureAtlas->bind();
    // The allowable values are 1 (byte-alignment), 2 (rows aligned to even-numbered bytes), Default: 4 (word-alignment), and 8 (rows start on double-word boundaries)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    textureAtlas->unbind();
}



void TextGenerator::createTextMesh(TriangleMesh<VertexNT, GLuint> &mesh, const std::string &text, int startX, int startY){

    int x=startX,y=startY;
    VertexNT verts[4];
    for(char c : text){
        character_info &info = characters[(int)c];

        vec3 offset = vec3(x+info.bl,y+info.bt-info.bh,0);


        //bottom left
        verts[0] = VertexNT(offset,
                            vec3(0,0,1),
                            vec2(info.tcMin.x,info.tcMax.y));
        //bottom right
        verts[1] = VertexNT(offset+vec3(info.bw,0,0),
                            vec3(0,0,1),
                            vec2(info.tcMax.x,info.tcMax.y));
        //top right
        verts[2] = VertexNT(offset+vec3(info.bw,info.bh,0),
                            vec3(0,0,1),
                            vec2(info.tcMax.x,info.tcMin.y));
        //top left
        verts[3] = VertexNT(offset+vec3(0,info.bh,0),
                            vec3(0,0,1),
                            vec2(info.tcMin.x,info.tcMin.y));

        x+=info.ax;
        y+=info.ay;
        mesh.addQuad(verts);
    }
}

DynamicText* TextGenerator::createDynamicText(int size, bool normalize){
    DynamicText* text = new DynamicText(size);

    text->texture = textureAtlas;

    std::string buffer;
    buffer.resize(size);
    buffer.assign(size,'A');

    createTextMesh(text->mesh,buffer);

    if(normalize){
        text->mesh.boundingBox.growBox(maxCharacter);
        aabb bb = text->mesh.getAabb();
        vec3 offset = bb.getPosition();
        mat4 t;
        t[3] = vec4(-offset,0);
        text->mesh.transform(t);
    }
    text->mesh.createBuffers(text->buffer);


    text->label = buffer;

    return text;
}

Text* TextGenerator::createText(const std::string &label, bool normalize){
    Text* text = new Text(label);

    text->texture = textureAtlas;

    createTextMesh(text->mesh,label);

    if(normalize){
        text->mesh.boundingBox.growBox(maxCharacter);
        aabb bb = text->mesh.getAabb();
        vec3 offset = bb.getPosition();
        mat4 t;
        t[3] = vec4(-offset,0);
        text->mesh.transform(t);
    }
    text->mesh.createBuffers(text->buffer);

    return text;
}

void TextGenerator::updateText(DynamicText* text, const std::string &l, int startIndex){
    std::string label(l);
    text->compressText(label,startIndex);
    if(label.size()==0){
        //no update needed
        return;
    }


    character_info &info = characters[(int)text->label[startIndex]];
    text->updateText(label,startIndex);

    //x offset of first new character
    int start = text->mesh.vertices[startIndex*4].position.x - info.bl;
    //delete everything from startindex to end
    text->mesh.vertices.resize(startIndex*4);
    text->mesh.faces.resize(startIndex);


    //calculate new faces
    createTextMesh(text->mesh,label,start);

    //update gl mesh
    text->updateGLBuffer(startIndex);


}
