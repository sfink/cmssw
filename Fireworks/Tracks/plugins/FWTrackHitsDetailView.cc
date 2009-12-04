// ROOT includes
#include "TEveStraightLineSet.h"
#include "TEvePointSet.h"
#include "TEveScene.h"
#include "TEveViewer.h"
#include "TGLEmbeddedViewer.h"
#include "TEveManager.h"
#include "TEveTrack.h"
//#include "TEveTrans.h"
#include "TEveText.h"
#include "TEveGeoShape.h"
#include "TGLFontManager.h"
#include "TGPack.h"
#include "TGeoBBox.h"
#include "TGSlider.h"
#include "TGLabel.h"
#include "TCanvas.h"
#include "TLatex.h"

// CMSSW includes
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"

// Fireworks includes
#include "Fireworks/Core/interface/FWModelId.h"
#include "Fireworks/Core/src/CmsShowMain.h"
#include "Fireworks/Core/interface/FWColorManager.h"

#include "Fireworks/Core/interface/FWEventItem.h"
#include "Fireworks/Core/interface/CSGAction.h"
#include "Fireworks/Core/interface/FWGUISubviewArea.h"
#include "Fireworks/Core/interface/FWIntValueListener.h"

#include "Fireworks/Tracks/plugins/FWTrackHitsDetailView.h"
#include "Fireworks/Tracks/plugins/TracksRecHitsUtil.h"
#include "Fireworks/Tracks/interface/TrackUtils.h"
#include "Fireworks/Tracks/interface/CmsMagField.h"

FWTrackHitsDetailView::FWTrackHitsDetailView ():
m_viewer(0),
m_modules(0),
m_slider(0),
m_sliderListener()
{
}

FWTrackHitsDetailView::~FWTrackHitsDetailView ()
{
    getEveWindow()->DestroyWindow();
}

void
FWTrackHitsDetailView::build (const FWModelId &id, const reco::Track* track, TEveWindowSlot* base)
{   
   TEveScene*  scene(0);
   TEveViewer* viewer(0);
   TCanvas*    canvas(0);
   TGVerticalFrame* guiFrame;
   TEveWindow* eveWindow = FWDetailViewBase::makePackViewerGui(base, canvas, guiFrame, viewer, scene);
   eveWindow->SetElementName("Track Hits Detail View");
   setEveWindow(eveWindow);
   m_viewer = (TGLEmbeddedViewer*)viewer->GetGLViewer();
   {
      TGCompositeFrame* f  = new TGVerticalFrame(guiFrame);
      guiFrame->AddFrame(f);
      f->AddFrame(new TGLabel(f, "Module Transparency:"), new TGLayoutHints(kLHintsLeft, 2, 2, 0, 0));
      m_slider = new TGHSlider(f, 120, kSlider1 | kScaleNo);
      f->AddFrame(m_slider, new TGLayoutHints(kLHintsTop | kLHintsLeft, 2, 2, 1, 4));
      m_slider->SetRange(0, 100);
      m_slider->SetPosition(75);

      m_sliderListener =  new FWIntValueListener();
      TQObject::Connect(m_slider, "PositionChanged(Int_t)", "FWIntValueListenerBase",  m_sliderListener, "setValue(Int_t)");
      m_sliderListener->valueChanged_.connect(boost::bind(&FWTrackHitsDetailView::transparencyChanged,this,_1));
   }

   {
      CSGAction* action = new CSGAction(this, "Show Module Labels");
      action->createCheckButton(guiFrame, new TGLayoutHints( kLHintsExpandX, 2, 3, 1, 4));
      action->activated.connect(sigc::mem_fun(this, &FWTrackHitsDetailView::rnrLabels));
   }
   {
      CSGAction* action = new CSGAction(this, "Pick Camera Center");
      action->createTextButton(guiFrame, new TGLayoutHints( kLHintsExpandX, 2, 3, 1, 4));
      action->activated.connect(sigc::mem_fun(this, &FWTrackHitsDetailView::pickCameraCenter));
   }

   TGCompositeFrame* p = (TGCompositeFrame*)guiFrame->GetParent();
   p->MapSubwindows();
   p->Layout();
    
   m_modules = new TEveElementList("Modules");
   scene->AddElement(m_modules);
   m_moduleLabels = new TEveElementList("Modules");
   scene->AddElement(m_moduleLabels);
   TracksRecHitsUtil::addHits(*track, id.item(), m_modules);
   for (TEveElement::List_i i=m_modules->BeginChildren(); i!=m_modules->EndChildren(); ++i)
   {
      TEveGeoShape* gs = dynamic_cast<TEveGeoShape*>(*i);
      gs->SetMainColor(kBlue);
      gs->SetMainTransparency(75);
      gs->SetPickable(kFALSE);

      TString name = gs->GetElementTitle();
      TEveText* text = new TEveText(name.Data());
      text->PtrMainTrans()->SetFrom(gs->RefMainTrans().Array());
      text->SetFontMode(TGLFont::kPixmap);
      text->SetFontSize(12);
      /*
        text->SetFontMode(TGLFont::kExtrude);
        Float_t textWidth = name.Length()*text->GetFontSize();

        TGeoBBox* bb = (TGeoBBox*)gs->GetShape();
        text->RefMainTrans().Move3LF(0, 0, +2*bb->GetDZ());
        text->PtrMainTrans()->RotateLF(2, 1, TMath::PiOver2());
        Double_t sx, sy, sz; text->PtrMainTrans()->GetScale(sx, sy, sz);
        Float_t minSide = TMath::Min(bb->GetDX(), bb->GetDY());
        Float_t a = minSide/textWidth;
        text->RefMainTrans().Scale(a, a, a);
      */
      m_moduleLabels->AddElement(text); 
   }

   CmsMagField* cmsMagField = new CmsMagField;
   cmsMagField->setReverseState( true );
   cmsMagField->setMagnetState( CmsShowMain::getMagneticField() > 0 );

   TEveTrackPropagator* prop = new TEveTrackPropagator();
   prop->SetMagFieldObj( cmsMagField );
   prop->SetStepper(TEveTrackPropagator::kRungeKutta);
   prop->SetMaxR(123);
   prop->SetMaxZ(300);
   TEveTrack* trk = fireworks::prepareTrack( *track, prop,
                                             id.item()->defaultDisplayProperties().color() );
   trk->MakeTrack();
   trk->SetLineWidth(2);
   prop->SetRnrDaughters(kTRUE);
   prop->SetRnrDecay(kTRUE);
   prop->SetRnrReferences(kTRUE);
   prop->SetRnrDecay(kTRUE);
   prop->SetRnrFV(kTRUE);
   scene->AddElement(trk);

   //LatB
   std::vector<TVector3> pixelPoints;
   std::vector<TVector3> monoPoints;
   std::vector<TVector3> stereoPoints;
   fireworks::pushPixelHits(pixelPoints, id, *track);
   fireworks::pushSiStripHits(monoPoints, stereoPoints, id, *track);
   TEveElementList* list = new TEveElementList("hits");
   fireworks::addTrackerHits3D(pixelPoints, list, kRed, 2);
   fireworks::addTrackerHits3D(monoPoints, list, kRed, 2);
   fireworks::addTrackerHits3D(stereoPoints, list, kRed, 2);
   scene->AddElement(list);
   //LatB

   scene->Repaint(true);

   m_viewer->UpdateScene();
   m_viewer->CurrentCamera().Reset();
   m_viewer->RequestDraw(TGLRnrCtx::kLODHigh);
   //   m_viewer->SetStyle(TGLRnrCtx::kOutline);
   m_viewer->SetDrawCameraCenter(kTRUE);

   addInfo(canvas);
}

void
FWTrackHitsDetailView::setBackgroundColor(Color_t col)
{
   FWColorManager::setColorSetViewer(m_viewer, col);
}


void
FWTrackHitsDetailView::pickCameraCenter()
{
   m_viewer->PickCameraCenter();
}

void
FWTrackHitsDetailView::transparencyChanged(int x)
{
   for (TEveElement::List_i i=m_modules->BeginChildren(); i!=m_modules->EndChildren(); ++i)
   {
      (*i)->SetMainTransparency(x);
   }
   gEve->Redraw3D();
}

void
FWTrackHitsDetailView::addInfo(TCanvas* canvas)
{
   canvas->cd();

   Double_t fontSize = 0.07;
   TLatex* latex = new TLatex();
   latex->SetTextSize(fontSize);
   Double_t y = 0.9;
   Double_t x = 0.02;
   Double_t yStep = 0.04;
   latex->DrawLatex(x, y, "Track hits info:");
   y -= yStep;

   Float_t r = 0.02;
   drawCanvasDot(x + r, y, r, kCyan);
   y -= r*0.5;
   latex->DrawLatex(x+ 3*r, y, "Camera center");
}

void
FWTrackHitsDetailView::rnrLabels()
{
   m_moduleLabels->SetRnrChildren(!m_moduleLabels->GetRnrChildren());
   gEve->Redraw3D();
}

REGISTER_FWDETAILVIEW(FWTrackHitsDetailView, Hits);
