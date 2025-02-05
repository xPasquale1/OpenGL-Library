#pragma once

#include <fstream>
#include "util.h"
#include "math.h"

//TODO namespace

struct TableOffset{
	DWORD tag;
	DWORD offset;
};

WORD swapEndian(WORD* val)noexcept{
    BYTE* out = (BYTE*)val;
    return (out[0]<<8) | out[1];
}

SWORD swapEndian(SWORD* val)noexcept{
    BYTE* out = (BYTE*)val;
    return (WORD)(out[0]<<8) | out[1];
}

DWORD swapEndian(DWORD* val)noexcept{
    BYTE* out = (BYTE*)val;
    return (out[0]<<24) | (out[1]<<16) | (out[2]<<8) | out[3];
}

constexpr DWORD tableStringToCode(const char* name)noexcept{
    return (name[3]<<24) | (name[2]<<16) | (name[1]<<8) | name[0];
}

bool flagBitSet(DWORD val, BYTE pos){
	return (val>>pos)&1;
}

BYTE readUint8(std::fstream& file){
    BYTE ret;
    file.read((char*)&ret, 1);
    return ret;
}

WORD readUint16(std::fstream& file){
    WORD ret;
    file.read((char*)&ret, 2);
    return swapEndian(&ret);
}

SWORD readInt16(std::fstream& file){
    SWORD ret;
    file.read((char*)&ret, 2);
    return swapEndian(&ret);
}

DWORD readUint32(std::fstream& file){
    DWORD ret;
    file.read((char*)&ret, 4);
    return swapEndian(&ret);
}

enum POINTTYPE : BYTE{
	OFFCURVE=0,
	ONCURVE=1
};
struct Glyphpoint{
	POINTTYPE type;
	SWORD x;
	SWORD y;
};

struct GlyphTriangle{
	SWORD x1;
	SWORD y1;
	SWORD x2;
	SWORD y2;
	SWORD x3;
	SWORD y3;
};

struct Glyph{
	SWORD numPoints = 0;
	Glyphpoint* points = nullptr;
	SWORD xMin = 0;
	SWORD yMin = 0;
	SWORD xMax = 0;
	SWORD yMax = 0;
	SWORD numContours = 0;
	WORD* endOfContours = nullptr;
    GlyphTriangle* triangles = nullptr;
	WORD numTriangles;
};

void readCoordinates(std::fstream& file, const BYTE* flags, Glyphpoint* coords, BYTE isY, SWORD numPoints)noexcept{
	if(numPoints <= 0) return;
	SWORD prevVal = 0;
	POINTTYPE prevType = OFFCURVE;			//TODO aktuell geht das davon aus, dass jede Kontur mit einem on-curve-point anfängt
	WORD realPointNumber = 0;
	for(SWORD i=0; i < numPoints; ++i){
		POINTTYPE type = (POINTTYPE)flagBitSet(flags[i], 0);
		SWORD tmpCoord;
		if(flagBitSet(flags[i], 1+isY)){
			tmpCoord = readUint8(file);
			if(!flagBitSet(flags[i], 4+isY)) tmpCoord = 0-tmpCoord;
		}else{
			if(flagBitSet(flags[i], 4+isY)){
				*((&coords[realPointNumber].x)+isY) = prevVal;
				coords[realPointNumber].type = type;
				realPointNumber++;
                prevType = type;
				continue;
			}
			tmpCoord = readInt16(file);
		}
		SWORD coord = prevVal + tmpCoord;
		prevType = type;
		*((&coords[realPointNumber].x)+isY) = coord;
		prevVal = coord;
		coords[realPointNumber].type = type;
		realPointNumber++;
	}
}

//Allokiert nur die Konturen
void createGlyph(Glyph& glyph, SWORD numContours)noexcept{
	glyph.numContours = numContours;
	glyph.endOfContours = new WORD[numContours];
}

void destroyGlyph(Glyph& glyph)noexcept{
	glyph.numPoints = 0;
	delete[] glyph.points;
	glyph.points = nullptr;
	glyph.numContours = 0;
	delete[] glyph.endOfContours;
	glyph.endOfContours = nullptr;
    delete[] glyph.triangles;
    glyph.triangles = nullptr;
    glyph.numTriangles = 0;
}

struct GlyphStorage{
    WORD glyphCount = 0;
    Glyph* glyphs = nullptr;
};

void createGlyphStorage(GlyphStorage& storage, WORD glyphCount)noexcept{
    storage.glyphCount = glyphCount;
    storage.glyphs = new Glyph[glyphCount];
}

void destroyGlyphStorage(GlyphStorage& storage)noexcept{
    for(WORD i=0; i < storage.glyphCount; ++i){
        destroyGlyph(storage.glyphs[i]);
    }
    delete[] storage.glyphs;
    storage.glyphs = nullptr;
}

void readSimpleGlyph(std::fstream& file, Glyph& glyph, SWORD numberOfContours)noexcept{
	glyph.xMin = readInt16(file);
	glyph.yMin = readInt16(file);
	glyph.xMax = readInt16(file);
	glyph.yMax = readInt16(file);
	WORD endPtsOfContours[numberOfContours];
	for(SWORD i=0; i < numberOfContours; ++i){
		endPtsOfContours[i] = readUint16(file);
	}
	WORD numPoints = endPtsOfContours[numberOfContours-1] + 1;
	createGlyph(glyph, numberOfContours);
	for(SWORD i=0; i < numberOfContours; ++i){
		glyph.endOfContours[i] = endPtsOfContours[i];
	}
	WORD instructionLength = readUint16(file);
	BYTE instructions[instructionLength];
	for(WORD i=0; i < instructionLength; ++i){
		instructions[i] = readUint8(file);
	}
	BYTE flags[numPoints];
	for(WORD i=0; i < numPoints; ++i){
		flags[i] = readUint8(file);
		if(flagBitSet(flags[i], 3)){
			BYTE flag = flags[i];
            BYTE toSkip = readUint8(file);
			for(BYTE j=0; j < toSkip; ++j){
				flags[++i] = flag;
			}
		}
	}
	Glyphpoint buffer[numPoints];
	readCoordinates(file, flags, buffer, 0, numPoints);
	readCoordinates(file, flags, buffer, 1, numPoints);
    
    //Berechne implizite Punkte
    Glyphpoint pointBuffer[numPoints*2];
    WORD startIdx = 0;
    WORD realNumPoints = 0;
    for(SWORD i=0; i < numberOfContours; ++i){
        WORD endIdx = glyph.endOfContours[i];
        pointBuffer[realNumPoints++] = buffer[startIdx];
        for(WORD j=startIdx+1; j <= endIdx; ++j){
            if(buffer[j].type == buffer[j-1].type){
                pointBuffer[realNumPoints].x = (buffer[j].x + buffer[j-1].x)/2;
                pointBuffer[realNumPoints].y = (buffer[j].y + buffer[j-1].y)/2;
                pointBuffer[realNumPoints].type = (POINTTYPE)!buffer[j].type;
                realNumPoints++;
            }
            pointBuffer[realNumPoints++] = buffer[j];
        }
        if(buffer[endIdx].type == buffer[startIdx].type){
            pointBuffer[realNumPoints].x = (buffer[endIdx].x + buffer[startIdx].x)/2;
            pointBuffer[realNumPoints].y = (buffer[endIdx].y + buffer[startIdx].y)/2;
            pointBuffer[realNumPoints].type = (POINTTYPE)!buffer[endIdx].type;
            realNumPoints++;
        }
        glyph.endOfContours[i] = realNumPoints-1;
        startIdx = endIdx + 1;
    }

    //Kopiere alle Punkte in den Glyphen
    delete[] glyph.points;
    glyph.points = new Glyphpoint[realNumPoints];
    glyph.numPoints = realNumPoints;
    for(WORD i=0; i < realNumPoints; ++i) glyph.points[i] = pointBuffer[i];
}

void readCompoundGlyph(std::fstream& file)noexcept{
    DWORD fileOffset = file.tellg();
    file.seekg(fileOffset+8, std::ios::beg);  //Skippe bounding Box
    WORD flags = readUint16(file);
    WORD offset = 6;
    if(flagBitSet(flags, 0)){
        offset += 2;
    }
    DWORD currentOffset = file.tellg();
    file.seekg(currentOffset+offset, std::ios::beg);  //Skippe den Rest
}

struct HorMetric{
    WORD advanceWidth;
    SWORD leftSideBearing;
};

//TODO sollte eine Mapping-Tabelle für alle Unicode Zeichen haben, kann man vllt mit einer Hashmap umsetzen, da es ja nicht alle Unicode-Zeichen geben muss
struct Font{
    WORD asciiToGlyphMapping[256];
    WORD unitsPerEm;		//TODO eigentlich wird das ja nie benutzt
	WORD pixelSize = 16;
    SWORD xMin;
    SWORD yMin;
    SWORD xMax;
    SWORD yMax;
    GlyphStorage glyphStorage;
    WORD horMetricsCount = 0;
    HorMetric* horMetrics = nullptr;
};

void destroyFont(Font& font){
    destroyGlyphStorage(font.glyphStorage);
    delete[] font.horMetrics;
    font.horMetricsCount = 0;
}

bool pointInTriangle(SWORD x1, SWORD y1, SWORD x2, SWORD y2, SWORD x3, SWORD y3, SWORD x, SWORD y){
    float totalArea = 1.f/((x2-x1)*(y3-y2)-(y2-y1)*(x3-x2));
    float m1 = ((x2-x)*(y3-y)-(x3-x)*(y2-y))*totalArea;
    float m2 = ((x3-x)*(y1-y)-(x1-x)*(y3-y))*totalArea;
    if(m1 >= 0 && m2 >= 0 && (m1+m2) <= 1) return true;
    return false;
}

bool lineSegmentIntersection(SWORD xBeg1, SWORD yBeg1, SWORD xEnd1, SWORD yEnd1, SWORD xBeg2, SWORD yBeg2, SWORD xEnd2, SWORD yEnd2)noexcept{
    if(xBeg1 == xBeg2 && yBeg1 == yBeg2 && xEnd1 == xEnd2 && yEnd1 == yEnd2) return true;
    if(xBeg1 == xEnd2 && yBeg1 == yEnd2 && xEnd1 == xBeg2 && yEnd1 == yBeg2) return true;
    int dx1 = xEnd1 - xBeg1;
    int dy1 = yEnd1 - yBeg1;
    int dx2 = xEnd2 - xBeg2;
    int dy2 = yEnd2 - yBeg2;

    int denom = dy2*dx1-dx2*dy1;
    if(denom == 0){     //Linien parallel, teste ob einer der Punkte in der jeweils anderen Linie liegt
        float tx1 = (xBeg2 - xBeg1)/(float)dx1;
        float ty1 = (yBeg2 - yBeg1)/(float)dy1;
        float tx2 = (xEnd2 - xBeg1)/(float)dx1;
        float ty2 = (yEnd2 - yBeg1)/(float)dy1;
        float tx3 = (xBeg1 - xBeg2)/(float)dx2;
        float ty3 = (yBeg1 - yBeg2)/(float)dy2;
        float tx4 = (xEnd1 - xBeg2)/(float)dx2;
        float ty4 = (yEnd1 - yBeg2)/(float)dy2;
        if(tx1 >= 0 && tx1 <= 1 && ty1 >= 0 && ty1 <= 1) return true;
        if(tx2 >= 0 && tx2 <= 1 && ty2 >= 0 && ty2 <= 1) return true;
        if(tx3 >= 0 && tx3 <= 1 && ty3 >= 0 && ty3 <= 1) return true;
        if(tx4 >= 0 && tx4 <= 1 && ty4 >= 0 && ty4 <= 1) return true;
        return false;
    }

    int dxBeg = xBeg2-xBeg1;
    int dyBeg = yBeg2-yBeg1;

    float t1 = (dy2*dxBeg-dx2*dyBeg)/(float)denom;
    float t2 = (dy1*dxBeg-dx1*dyBeg)/(float)denom;

    return (t1 >= 0 && t1 <= 1 && t2 >= 0 && t2 <= 1);
}

bool lineSegmentCrossing(SWORD xBeg1, SWORD yBeg1, SWORD xEnd1, SWORD yEnd1, SWORD xBeg2, SWORD yBeg2, SWORD xEnd2, SWORD yEnd2)noexcept{
    if(xBeg1 == xBeg2 && yBeg1 == yBeg2 && xEnd1 == xEnd2 && yEnd1 == yEnd2) return true;
    if(xBeg1 == xEnd2 && yBeg1 == yEnd2 && xEnd1 == xBeg2 && yEnd1 == yBeg2) return true;
    int dx1 = xEnd1 - xBeg1;
    int dy1 = yEnd1 - yBeg1;
    int dx2 = xEnd2 - xBeg2;
    int dy2 = yEnd2 - yBeg2;

    int denom = dy2*dx1-dx2*dy1;
    if(denom == 0){     //Linien parallel, teste ob einer der Punkte in der jeweils anderen Linie liegt
        float tx1 = (xBeg2 - xBeg1)/(float)dx1;
        float ty1 = (yBeg2 - yBeg1)/(float)dy1;
        float tx2 = (xEnd2 - xBeg1)/(float)dx1;
        float ty2 = (yEnd2 - yBeg1)/(float)dy1;
        float tx3 = (xBeg1 - xBeg2)/(float)dx2;
        float ty3 = (yBeg1 - yBeg2)/(float)dy2;
        float tx4 = (xEnd1 - xBeg2)/(float)dx2;
        float ty4 = (yEnd1 - yBeg2)/(float)dy2;
        if(tx1 > 0 && tx1 < 1 && ty1 > 0 && ty1 < 1) return true;
        if(tx2 > 0 && tx2 < 1 && ty2 > 0 && ty2 < 1) return true;
        if(tx3 > 0 && tx3 < 1 && ty3 > 0 && ty3 < 1) return true;
        if(tx4 > 0 && tx4 < 1 && ty4 > 0 && ty4 < 1) return true;
        return false;
    }

    int dxBeg = xBeg2-xBeg1;
    int dyBeg = yBeg2-yBeg1;

    float t1 = (dy2*dxBeg-dx2*dyBeg)/(float)denom;
    float t2 = (dy1*dxBeg-dx1*dyBeg)/(float)denom;

    return (t1 > 0 && t1 < 1 && t2 > 0 && t2 < 1);
}

void addContour(WORD startIdx, WORD endIdx, std::vector<WORD>& indices, Glyph& glyph){
    for(WORD i=startIdx; i <= endIdx; ++i){
        indices.push_back(i);
    }
    indices.push_back(startIdx);
}

struct Edge{
    SWORD x1;
    SWORD y1;
    SWORD x2;
    SWORD y2;
};

bool testEdges(Glyph& glyph, WORD contourIdx, WORD pointIdx, std::vector<Edge>& bridges){
    Glyphpoint& point = glyph.points[glyph.endOfContours[contourIdx-1]+1];      //Anfangsindex der zu testenden Lochkontur TODO sollte vllt mal alle Punkte testen
    WORD pointIdxHole = glyph.endOfContours[contourIdx-1]+1;
    SWORD bridgeDirX = point.x-glyph.points[pointIdx].x;
    SWORD bridgeDirY = point.y-glyph.points[pointIdx].y;
    SWORD prevDirX = glyph.points[pointIdx-1].x-glyph.points[pointIdx].x;
    SWORD prevDirY = glyph.points[pointIdx-1].y-glyph.points[pointIdx].y;
    SWORD nextDirX = glyph.points[pointIdx+1].x-glyph.points[pointIdx].x;
    SWORD nextDirY = glyph.points[pointIdx+1].y-glyph.points[pointIdx].y;
    SDWORD cross1 = bridgeDirX * prevDirY - bridgeDirY * prevDirX;
    SDWORD cross2 = bridgeDirX * nextDirY - bridgeDirY * nextDirX;
    if(cross1 > 0 || cross2 < 0) return false;
    for(WORD k=0; k < bridges.size(); ++k){
        if(lineSegmentCrossing(bridges[k].x1, bridges[k].y1, bridges[k].x2, bridges[k].y2, point.x, point.y, glyph.points[pointIdx].x, glyph.points[pointIdx].y)) return false;
    }

    WORD startIdx = 0;
    for(SWORD j=0; j < glyph.numContours; ++j){
        WORD endIdx = glyph.endOfContours[j];
        for(WORD k=startIdx+1; k <= endIdx; ++k){
            if(k == pointIdx) continue;
            if(k == pointIdxHole) continue;
            if(k-1 == pointIdx) continue;
            if(k-1 == pointIdxHole) continue;
            if(lineSegmentIntersection(glyph.points[k].x, glyph.points[k].y, glyph.points[k-1].x, glyph.points[k-1].y, point.x, point.y, glyph.points[pointIdx].x, glyph.points[pointIdx].y)) return false;
        }
        if(endIdx != pointIdx && endIdx != pointIdxHole && startIdx != pointIdx && startIdx != pointIdxHole){
            if(lineSegmentIntersection(glyph.points[endIdx].x, glyph.points[endIdx].y, glyph.points[startIdx].x, glyph.points[startIdx].y, point.x, point.y, glyph.points[pointIdx].x, glyph.points[pointIdx].y)) return false;
        }
        startIdx = endIdx + 1;
    }
    bridges.push_back({point.x, point.y, glyph.points[pointIdx].x, glyph.points[pointIdx].y});
    return true;
}

struct OutlineContour{
    WORD index;
    std::vector<WORD> indices;
};

void triangulateGlyph(Font& font, BYTE c, std::vector<GlyphTriangle>& triangles){
    Glyph& glyph = font.glyphStorage.glyphs[font.asciiToGlyphMapping[c]];
    std::vector<OutlineContour> outlines;
    std::vector<WORD> holes;                   //Speicher die Indexe der "Löcher" Konturen
    std::vector<Edge> bridges;
    WORD startIdx = 0;
    for(SWORD i=0; i < glyph.numContours; ++i){     //Berechnet die winding-order der Konturen, negativ ist mit dem Uhrzeigersinn, positiv gegen
        WORD endIdx = glyph.endOfContours[i];
        SDWORD sign = 0;
        for(WORD j=startIdx+1; j <= endIdx; ++j){
            sign += (glyph.points[j].x-glyph.points[j-1].x)*(glyph.points[j].y+glyph.points[j-1].y);
        }
        sign += (glyph.points[startIdx].x-glyph.points[endIdx].x)*(glyph.points[endIdx].y+glyph.points[startIdx].y);
        if(sign >= 0){
            OutlineContour outline;
            outline.index = i;
            outlines.push_back(std::move(outline));     //TODO idk ob std::move nötig ist
        }else{
            holes.push_back(i);
        }
        startIdx = endIdx + 1;
    }

    for(WORD i=0; i < outlines.size(); ++i){
        OutlineContour& outline = outlines[i];
        WORD startIdx = outline.index > 0 ? glyph.endOfContours[outline.index-1]+1 : 0;
        WORD endIdx = glyph.endOfContours[outline.index];
        std::vector<WORD> remainingContours;
        for(WORD j=0; j < holes.size(); ++j) remainingContours.push_back(holes[j]);
        for(WORD j=startIdx; j <= endIdx; ++j){
            outline.indices.push_back(j);
            if(remainingContours.size() > 0){
                for(size_t k=0; k < remainingContours.size(); ++k){
                    WORD contourIdx = remainingContours[k];
                    if(j == startIdx) continue;     //TODO nur hier weil ich zu faul bin den Fall im testEdges abzudecken
                    if(!testEdges(glyph, contourIdx, j, bridges)) continue;
                    addContour(glyph.endOfContours[contourIdx-1]+1, glyph.endOfContours[contourIdx], outline.indices, glyph);
                    remainingContours.erase(remainingContours.begin()+k);
                    outline.indices.push_back(j);
                    break;
                }
            }
        }
        outline.indices.push_back(startIdx);
    }

    for(WORD i=0; i < outlines.size(); ++i){
        OutlineContour& outline = outlines[i];
        while(outline.indices.size() > 3){
            for(WORD j=1; j < outline.indices.size(); ++j){
                WORD n = outline.indices.size();
                WORD current = outline.indices[j];
                WORD prev = outline.indices[(j-1+n)%n];
                WORD next = outline.indices[(j+1)%n];
                Glyphpoint& p = glyph.points[current];
                Glyphpoint& p1 = glyph.points[prev];
                Glyphpoint& p2 = glyph.points[next];
                bool valid = true;
                SWORD dx1 = p1.x - p.x;
                SWORD dy1 = p1.y - p.y;
                SWORD dx2 = p2.x - p.x;
                SWORD dy2 = p2.y - p.y;
                SDWORD sign = dx1 * dy2 - dy1 * dx2;
                if(sign <= 0) continue;
                for(WORD k=0; k < outline.indices.size(); ++k){
                    Glyphpoint& point = glyph.points[outline.indices[k]];
                    if(&point == &p || &point == &p1 || &point == &p2) continue;
                    if(pointInTriangle(p.x, p.y, p1.x, p1.y, p2.x, p2.y, point.x, point.y)){
                        valid = false;
                        break;
                    }
                }
                if(!valid) continue;
                triangles.push_back({(SWORD)(p.x), (SWORD)(p.y), (SWORD)(p1.x), (SWORD)(p1.y), (SWORD)(p2.x), (SWORD)(p2.y)});
                outline.indices.erase(outline.indices.begin()+j);
            }
        }
    }
}

ErrCode loadTTF(Font& font, const char* name, WORD pixelSize = 32)noexcept{
	std::fstream file;
    file.open(name, std::ios::in | std::ios::binary);
    if(!file.is_open()) return FILE_NOT_FOUND;
    file.seekp(0, std::ios::end);
    std::cout << "Dateigröße in Bytes: " << file.tellp() << std::endl;

	Hashmap map;
	createHashmap(map, 100);

    file.seekg(4, std::ios::beg);
    WORD numTables = readUint16(file);
    std::cout << "Tabellen Anzahl: " << numTables << std::endl;
	TableOffset tableData[numTables];
    WORD seekOffset = 12;
    for(WORD i=0; i < numTables; ++i){
        file.seekg(seekOffset, std::ios::beg);
        DWORD tag;
        file.read((char*)&tag, 4);
        file.seekg(seekOffset+8, std::ios::beg);
        DWORD offset = readUint32(file);
		tableData[i].offset = offset;
		tableData[i].tag = tag;
		insertHashmap(map, tag, &tableData[i]);
        seekOffset += 16;
    }

    TableOffset* head = (TableOffset*)searchHashmap(map, tableStringToCode("head"));
    if(head == nullptr){
        destroyHashmap(map);
        file.close();
        return GENERIC_ERROR;
    }
    file.seekg(head->offset, std::ios::beg);
    file.seekg(18, std::ios::cur);  //Alle möglichen Headerdaten skippen
    font.unitsPerEm = readUint16(file);
    file.seekg(8, std::ios::cur);  //Mehr Headerdaten skippen
    font.xMin = readInt16(file);
    font.yMin = readInt16(file);
    font.xMax = readInt16(file);
    font.yMax = readInt16(file);
    file.seekg(14, std::ios::cur);  //Noch mehr Headerdaten skippen
    SWORD indexToLocFormat = readInt16(file);

	TableOffset* maxp = (TableOffset*)searchHashmap(map, tableStringToCode("maxp"));
	if(maxp == nullptr){
		destroyHashmap(map);
		file.close();
		return GENERIC_ERROR;
	}
	file.seekg(maxp->offset, std::ios::beg);
	DWORD version = readUint32(file);
	WORD numGlyphs = readUint16(file);
	std::cout << "Glyphenanzahl: " << numGlyphs << std::endl;
    
    createGlyphStorage(font.glyphStorage, numGlyphs);
	WORD maxPoints = readUint16(file);
	std::cout << "Maximale Punkteanzahl: " << maxPoints << std::endl;
	WORD maxContours = readUint16(file);
	std::cout << "Maximale Konturenanzahl: " << maxContours << std::endl;

    TableOffset* loca = (TableOffset*)searchHashmap(map, tableStringToCode("loca"));
    if(loca == nullptr){
        destroyHashmap(map);
        file.close();
        return GENERIC_ERROR;
    }
    file.seekg(loca->offset, std::ios::beg);
    DWORD glyphOffsets[numGlyphs];
    bool emptyGlyphs[numGlyphs]{false};
    for(WORD i=0; i <= numGlyphs; ++i){     //Ja laut Doku numGlyphs + 1
        if(indexToLocFormat==0){
            WORD offset = readUint16(file);
            glyphOffsets[i] = offset*2;
        }else{
            glyphOffsets[i] = readUint32(file);
        }
        if(i < 1) continue;
        WORD length = glyphOffsets[i] - glyphOffsets[i-1];
        if(length == 0) emptyGlyphs[i-1] = true;
    }

	TableOffset* glyf = (TableOffset*)searchHashmap(map, tableStringToCode("glyf"));
	if(glyf == nullptr){
		destroyHashmap(map);
		file.close();
		return GENERIC_ERROR;
	}

    for(WORD i=0; i < numGlyphs; ++i){
        DWORD offsetForGlyph = glyphOffsets[i] + glyf->offset;
        file.seekg(offsetForGlyph, std::ios::beg);
        SWORD numberOfContours = readInt16(file);
        if(numberOfContours > 0 && emptyGlyphs[i] == false){
            readSimpleGlyph(file, font.glyphStorage.glyphs[i], numberOfContours);
        }else if(numberOfContours == 0){
            //TODO uhh... das ist ein simple Glyph laut Doku
        }else{
            readCompoundGlyph(file);
        }
    }

    TableOffset* cmap =(TableOffset*)searchHashmap(map, tableStringToCode("cmap"));
    if(cmap == nullptr){
        destroyHashmap(map);
        file.close();
        return GENERIC_ERROR;
    }
    file.seekg(cmap->offset, std::ios::beg);
    file.seekg(2, std::ios::cur);				//Skippe Version
    WORD numberSubtables = readUint16(file);
    for(WORD i=0; i < numberSubtables; ++i){
        WORD platformID = readUint16(file);
        WORD platformSpecificID = readUint16(file);
        DWORD offset = readUint32(file);
        //TODO Sucht aktuell nur nach einer Unicode Tabelle die angeblich am meisten verwendet wird
        if(platformID == 0 && platformSpecificID == 3){
            file.seekg(cmap->offset+offset, std::ios::beg);
            WORD format = readUint16(file);
            //TODO auch hier wieder nur ein Format, nämlich 4, da es das meist genutzte ist
            if(format == 4){
                WORD length = readUint16(file);
                WORD language = readUint16(file);
                WORD segCountX2 = readUint16(file);
                WORD searchRange = readUint16(file);
                WORD entrySelector = readUint16(file);
                WORD rangeShift = readUint16(file);
                WORD* endCode = new WORD[segCountX2/2];
                for(WORD j=0; j < segCountX2/2; ++j){
                    endCode[j] = readUint16(file);
                }
                file.seekg(2, std::ios::cur);				//Skippe padding
                WORD* startCode = new WORD[segCountX2/2];
                for(WORD j=0; j < segCountX2/2; ++j){
                    startCode[j] = readUint16(file);
                }
                WORD* idDelta = new WORD[segCountX2/2];
                for(WORD j=0; j < segCountX2/2; ++j){
                    idDelta[j] = readUint16(file);
                }
                WORD* idRangeOffset = new WORD[segCountX2/2];
                for(WORD j=0; j < segCountX2/2; ++j){
                    idRangeOffset[j] = readUint16(file);
                }
                WORD restBytes = length - (16+4*segCountX2);
                WORD* glyphIndexArray = new WORD[restBytes];
                for(WORD j=0; j < restBytes; ++j){
                    glyphIndexArray[j] = readUint16(file);
                }
                for(WORD j=0; j < 256; ++j){
                    for(WORD k=0; k < segCountX2/2; ++k){
                        if(endCode[k] >= j){				//Missing Char Symbol
                            if(startCode[k] > j){
                                font.asciiToGlyphMapping[j] = 0;
                                break;
                            }
                            if(idRangeOffset[k] == 0){
                                font.asciiToGlyphMapping[j] = idDelta[k] + j;
                                break;
                            }
                            WORD offset = (idRangeOffset[k]-segCountX2+k*2)/2;
                            font.asciiToGlyphMapping[j] = glyphIndexArray[offset + (j-startCode[k])];
                            break;
                        }
                    }
                }
                delete[] startCode;
                delete[] idDelta;
                delete[] idRangeOffset;
                delete[] glyphIndexArray;
                break;
            }
        }
    }

    TableOffset* hhea = (TableOffset*)searchHashmap(map, tableStringToCode("hhea"));
    if(cmap == nullptr){
        destroyHashmap(map);
        file.close();
        return GENERIC_ERROR;
    }
    file.seekg(hhea->offset, std::ios::beg);
    file.seekg(34, std::ios::cur);					//Skippe mal wieder ne Menge Daten
    WORD numOfLongHorMetrics = readUint16(file);

    TableOffset* hmtx = (TableOffset*)searchHashmap(map, tableStringToCode("hmtx"));
    if(cmap == nullptr){
        destroyHashmap(map);
        file.close();
        return GENERIC_ERROR;
    }
    file.seekg(hmtx->offset, std::ios::beg);
    font.horMetricsCount = numOfLongHorMetrics;
    font.horMetrics = new HorMetric[numOfLongHorMetrics];
    for(WORD i=0; i < numOfLongHorMetrics; ++i){
        font.horMetrics[i].advanceWidth = readUint16(file);
        font.horMetrics[i].leftSideBearing = readInt16(file);
    }

	//Berechne yMin und yMax für die ASCII Glyphen
    SWORD yMin = font.glyphStorage.glyphs[font.asciiToGlyphMapping[0]].yMin;
    SWORD yMax = font.glyphStorage.glyphs[font.asciiToGlyphMapping[0]].yMax;
    for(WORD i=1; i < 256; ++i){
        SWORD min = font.glyphStorage.glyphs[font.asciiToGlyphMapping[i]].yMin;
        SWORD max = font.glyphStorage.glyphs[font.asciiToGlyphMapping[i]].yMax;
        if(min < yMin) yMin = min;
        if(max > yMax) yMax = max;
    }
	font.yMax = yMax;
	font.yMin = yMin;

    for(WORD i=0; i < 256; ++i){
        std::vector<GlyphTriangle> triangles;
        triangulateGlyph(font, i, triangles);
        font.glyphStorage.glyphs[font.asciiToGlyphMapping[i]].triangles = new GlyphTriangle[triangles.size()];
        font.glyphStorage.glyphs[font.asciiToGlyphMapping[i]].numTriangles = triangles.size();
        for(std::size_t j=0; j < triangles.size(); ++j){
            font.glyphStorage.glyphs[font.asciiToGlyphMapping[i]].triangles[j] = triangles[j];
        }
    }

    font.pixelSize = pixelSize;

	destroyHashmap(map);
	file.close();
	return SUCCESS;
}
