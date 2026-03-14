#pragma once
#include "core/MaryLineage.hpp"
#include <random>

enum class TaskType
{
    COPY_PATTERN,
    SHORT_MEMORY,
    BINARY_CLASSIFICATION,
    STABILITY,
    COMMAND_RESPONSE
};

struct TrainingTask
{
    TaskType type;

    std::vector<float> input;
    std::vector<float> expected;

    int difficulty = 0;
};

struct TaskResult
{
    float reward;
    float error;
    bool success;
};

class TaskGenerator
{
public:

    static TrainingTask generate(TaskType type,
                                 int vectorSize,
                                 std::mt19937& rng)
    {
        TrainingTask task;
        task.type = type;

        std::uniform_real_distribution<float> dist(0.0f,1.0f);

        task.input.resize(vectorSize);
        task.expected.resize(vectorSize);

        for(int i=0;i<vectorSize;i++)
        {
            float v = dist(rng);
            task.input[i] = v;
            task.expected[i] = v;
        }

        if(type == TaskType::BINARY_CLASSIFICATION)
        {
            float sum = 0;

            for(auto v : task.input)
                sum += v;

            task.expected = { sum > vectorSize*0.5f ? 1.0f : 0.0f };
        }

        return task;
    }
};

class TaskEvaluator
{
public:

    static TaskResult evaluate(
        const TrainingTask& task,
        const std::vector<float>& output)
    {
        TaskResult result;

        float error = 0;

        int n = std::min(task.expected.size(), output.size());

        for(int i=0;i<n;i++)
            error += std::abs(task.expected[i] - output[i]);

        error /= n;

        result.error = error;
        result.reward = 1.0f - error;
        result.success = result.reward > 0.7f;

        return result;
    }
};

class LevelManager
{
public:

    static std::vector<TaskType> levelTasks(int level)
    {
        std::vector<TaskType> tasks;

        if(level == 1)
        {
            tasks =
            {
                TaskType::COPY_PATTERN,
                TaskType::SHORT_MEMORY,
                TaskType::BINARY_CLASSIFICATION,
                TaskType::STABILITY,
                TaskType::COMMAND_RESPONSE
            };
        }

        return tasks;
    }

};

class MaryUpbringing
{
public:

    struct TrainingStats
    {
        int levelReached = 0;
        float averageReward = 0;
        int tasksSolved = 0;
    };

    TrainingStats raiseDaughter(
        MaryLineage& mother,
        MaryLineage& daughter,
        int maxLevels = 100)
    {
        TrainingStats stats;

        std::mt19937 rng(std::random_device{}());

        int vectorSize = determineVectorSize(daughter);

        for(int level=1; level<=maxLevels; level++)
        {
            float levelReward = runLevel(
                mother,
                daughter,
                level,
                vectorSize,
                rng);

            if(levelReward < 0.6f)
                break;

            stats.levelReached = level;
            stats.averageReward = levelReward;
        }

        return stats;
    }

private:

    int determineVectorSize(MaryLineage&)
    {
        // позже можно связать с DeviceDescriptor
        return 16;
    }

    float runLevel(
        MaryLineage& mother,
        MaryLineage& daughter,
        int level,
        int vectorSize,
        std::mt19937& rng)
    {
        auto tasks = LevelManager::levelTasks(level);

        float rewardSum = 0;
        int count = 0;

        for(auto type : tasks)
        {
            for(int i=0;i<10;i++) // 10 задач каждого типа
            {
                auto task =
                    TaskGenerator::generate(type, vectorSize, rng);

                auto output =
                    daughter.coordinateComputation(
                        task.input,
                        {}
                    );

                auto result =
                    TaskEvaluator::evaluate(task, output);

                rewardSum += result.reward;
                count++;

                if(result.reward > 0.7f)
                    mother.rewardDaughter("daughter");
                else
                    mother.punishDaughter("daughter");
            }
        }

        return rewardSum / count;
    }
};