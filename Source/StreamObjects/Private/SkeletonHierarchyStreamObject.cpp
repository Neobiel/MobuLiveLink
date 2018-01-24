﻿#include "SkeletonHierarchyStreamObject.h"
#include "MobuLiveLinkUtilities.h"

SkeletonHierarchyStreamObject::SkeletonHierarchyStreamObject(const FBModel* ModelPointer, const TSharedPtr<ILiveLinkProvider> StreamProvider) :
	ModelStreamObject(ModelPointer, StreamProvider, { TEXT("Full Hierarchy"), TEXT("Root Only") })
{
	Refresh();
};

void SkeletonHierarchyStreamObject::Refresh() 
{
	BoneNames.Empty();
	BoneParents.Empty();
	BoneModels.Empty();

	BoneNames.Emplace(RootModel->Name);
	BoneParents.Emplace(-1);
	BoneModels.Emplace(RootModel);

	// If Streaming as Hierarchy
	if (StreamingMode == 0)
	{
		TArray<TPair<int, FBModel*>> SearchList;
		TArray<TPair<int, FBModel*>> SearchListNext;

		SearchList.Emplace(0, (FBModel*)RootModel);

		if (StreamingMode == 0)
		{
			while (SearchList.Num() > 0)
			{
				for (const auto& SearchPair : SearchList)
				{
					int ParentIdx = SearchPair.Key;
					FBModel* SearchModel = SearchPair.Value;
					int ChildCount = SearchModel->Children.GetCount(); // Yuck

					for (int ChildIdx = 0; ChildIdx < ChildCount; ++ChildIdx)
					{
						FBModel* ChildModel = SearchModel->Children[ChildIdx];

						if (ChildModel->GetTypeId() != FBModelSkeleton::TypeInfo) continue; // Only want joints

						BoneNames.Emplace(ChildModel->Name);
						BoneParents.Emplace(ParentIdx);
						BoneModels.Emplace(ChildModel);

						SearchListNext.Emplace(BoneModels.Num() - 1, ChildModel);
					}
				}
				SearchList = SearchListNext;
				SearchListNext.Empty();
			}
		}
	}
	Provider->UpdateSubject(SubjectName, BoneNames, BoneParents);
};

void SkeletonHierarchyStreamObject::UpdateSubjectFrame() 
{
	if (!bIsActive) return;

	int BoneCount = BoneNames.Num();
	TArray<FTransform> BoneTransforms;
	BoneTransforms.SetNum(BoneCount);

	TArray<FTransform> ParentInverseTransforms;
	ParentInverseTransforms.SetNum(BoneCount);

	TArray<FLiveLinkCurveElement> CurveData;

	// loop through children here
	for (int BoneIndex = 0; BoneIndex < BoneModels.Num(); ++BoneIndex)
	{
		BoneTransforms[BoneIndex] = UnrealTransformFromModel((FBModel*)BoneModels[BoneIndex]);
		ParentInverseTransforms[BoneIndex] = BoneTransforms[BoneIndex].Inverse();
		if (BoneParents[BoneIndex] != -1)
		{
			BoneTransforms[BoneIndex] = BoneTransforms[BoneIndex] * ParentInverseTransforms[BoneParents[BoneIndex]];
		}

		// Stream all parameters of all bones as "<BoneName>:<ParameterName>"
		CurveData.Append( GetAllAnimatableCurves((FBModel*)BoneModels[BoneIndex], BoneNames[BoneIndex].ToString()) );
	}



	FBTime LocalTime = FBSystem().LocalTime;
	Provider->UpdateSubjectFrame(SubjectName, BoneTransforms, CurveData, LocalTime.GetSecondDouble(), LocalTime.GetFrame());
};