// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UVGenerationTool.h"

#include "RawMesh.h"
#include "Engine/StaticMesh.h"
#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "MyXatlas.h"

#define LOCTEXT_NAMESPACE "FUVGenerationToolModule"

void FUVGenerationToolModule::StartupModule()
{
	SetupContentBrowserContextMenuExtender();
}

void FUVGenerationToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FUVGenerationToolModule::SetupContentBrowserContextMenuExtender()
{
	if (!IsRunningCommandlet())
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray< FContentBrowserMenuExtender_SelectedAssets >& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FUVGenerationToolModule::OnExtendContentBrowserAssetSelectionMenu));
	}
}

TSharedRef<FExtender>  FUVGenerationToolModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	TArray<UStaticMesh*> StaticMeshes;
	//ɸѡѡ����Դ�����ͣ���ǰɸѡ����UStaticMesh����
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClass == UStaticMesh::StaticClass()->GetFName())
		{
			if (UStaticMesh * SelectedStaticMesh = Cast<UStaticMesh>(Asset.GetAsset()))
			{
				StaticMeshes.Add(SelectedStaticMesh);
			}
		}
	}
	//���ѡ����һ�����ϵ�UStaticMesh��Դ����Ϊ������һ���Ҽ��˵�
	if (StaticMeshes.Num() > 0)
	{
		Extender->AddMenuExtension("GetAssetActions", EExtensionHook::First, nullptr, FMenuExtensionDelegate::CreateLambda(
			[StaticMeshes](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("Generate UV", "Generate UV"),//Ҫ��ʾ�Ĳ˵���ǩ��
				LOCTEXT("Tooltip", "Tool tip"),//�����ͣ�ڸò˵���ʱ����ʾ
				FSlateIcon(),//ʹ�õ�ͼ��
				FUIAction(FExecuteAction::CreateStatic(&FUVGenerationToolModule::GeneratUVExec, StaticMeshes), FCanExecuteAction()));//�˵�ִ�еĲ���
		}));
	}
	return Extender;
}

void FUVGenerationToolModule::GeneratUVExec(TArray<UStaticMesh*> StaticMeshes)
{
	for (int32 MeshNum = 0; MeshNum < StaticMeshes.Num(); ++MeshNum)
	{
		std::vector<float> Xatlas_v;
		std::vector<float> Xatlas_vn;
		std::vector<int> Xatlas_index;
		std::vector<MyXatlas::outVertex> NewVertex;//�������������

		FRawMesh InNewMesh;
		StaticMeshes[MeshNum]->GetSourceModel(0).LoadRawMesh(InNewMesh);

		/*��ȡStaticMesh������*/
		int OldVertexPositionsNum = InNewMesh.VertexPositions.Num();
		Xatlas_v.resize(OldVertexPositionsNum * 3);
		for (int i = 0; i < OldVertexPositionsNum;i++)
		{
			Xatlas_v[i * 3] = InNewMesh.VertexPositions[i].X;
			Xatlas_v[i * 3 + 1] = InNewMesh.VertexPositions[i].Y;
			Xatlas_v[i * 3 + 2] = InNewMesh.VertexPositions[i].Z;
		}
		int OldWedgeTangentNum = InNewMesh.WedgeTangentZ.Num();
		Xatlas_vn.resize(OldWedgeTangentNum * 3);
		for (int i = 0; i < OldWedgeTangentNum;i++)
		{
			Xatlas_vn[i * 3] = InNewMesh.WedgeTangentZ[i].X;
			Xatlas_vn[i * 3 + 1] = InNewMesh.WedgeTangentZ[i].X;
			Xatlas_vn[i * 3 + 2] = InNewMesh.WedgeTangentZ[i].X;
		}
		int OldWedgeIndicesNum = InNewMesh.WedgeIndices.Num();
		Xatlas_index.resize(OldWedgeIndicesNum);
		for (int i = 0; i < OldWedgeIndicesNum;i++)
			Xatlas_index[i] = InNewMesh.WedgeIndices[i] + 1;//Xatlas�е������Ǵ�1��ʼ��

		/*����UV*/
		NewVertex = MyXatlas::GenerateUV(Xatlas_v, Xatlas_vn, Xatlas_index);

		int NewWedgeIndicesNum = NewVertex[0].Index.size();
		if (NewVertex.size() == 0)
		{
			UE_LOG(LogClass, Log, TEXT("outV.size() == 0"));//����UVʧ��
			return;
		}

		//�����µĶ���λ��
		InNewMesh.VertexPositions.Empty();
		InNewMesh.VertexPositions.AddZeroed(NewVertex.size());
		for (int32 i = 0; i < InNewMesh.VertexPositions.Num(); ++i)
		{
			InNewMesh.VertexPositions[i] = FVector(NewVertex[i].outP.x, NewVertex[i].outP.y, NewVertex[i].outP.z);
		}

		//�����µĶ�������
		InNewMesh.WedgeIndices.Empty();
		InNewMesh.WedgeIndices.AddZeroed(NewWedgeIndicesNum);
		for (int i = 0;i < InNewMesh.WedgeIndices.Num();i++)
		{
			InNewMesh.WedgeIndices[i] = NewVertex[0].Index[i] - 1;
		}

		//�������� ����µ���������ԭ������������
		if (NewWedgeIndicesNum / 3 != InNewMesh.FaceMaterialIndices.Num())
		{
			InNewMesh.FaceMaterialIndices.Empty();
			InNewMesh.FaceMaterialIndices.AddZeroed(NewWedgeIndicesNum / 3);
			InNewMesh.FaceSmoothingMasks.Empty();
			InNewMesh.FaceSmoothingMasks.AddZeroed(NewWedgeIndicesNum / 3);
			for (int32 i = 0; i < 30; ++i)
			{
				InNewMesh.FaceMaterialIndices[i] = 0;
				InNewMesh.FaceSmoothingMasks[i] = 0;
			}
		}

		//�����µĶ�������ͷ���
		InNewMesh.WedgeTexCoords[0].Empty();
		InNewMesh.WedgeTexCoords[0].AddZeroed(NewWedgeIndicesNum);
		InNewMesh.WedgeTangentX.Empty();
		InNewMesh.WedgeTangentX.AddZeroed(NewWedgeIndicesNum);
		InNewMesh.WedgeTangentY.Empty();
		InNewMesh.WedgeTangentY.AddZeroed(NewWedgeIndicesNum);
		InNewMesh.WedgeTangentZ.Empty();
		InNewMesh.WedgeTangentZ.AddZeroed(NewWedgeIndicesNum);
		for (int32 i = 0; i < NewWedgeIndicesNum; ++i)
		{
			InNewMesh.WedgeTexCoords[0][i] = FVector2D(
				NewVertex[InNewMesh.WedgeIndices[i]].outT.u,
				NewVertex[InNewMesh.WedgeIndices[i]].outT.v);//����UV��Ϣ

			InNewMesh.WedgeTangentX[i] = FVector(0, 0, 0);
			InNewMesh.WedgeTangentY[i] = FVector(0, 0, 0);
			InNewMesh.WedgeTangentZ[i] = FVector(0, 0, 0);//���뷨����Ϣ
		}

		StaticMeshes[MeshNum]->GetSourceModel(0).SaveRawMesh(InNewMesh);
		StaticMeshes[MeshNum]->MarkPackageDirty();
		StaticMeshes[MeshNum]->Build();
		StaticMeshes[MeshNum]->PostEditChange();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUVGenerationToolModule, UVGenerationTool)